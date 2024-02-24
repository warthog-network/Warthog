#include "generator.hpp"
#include "db/chain_db.hpp"
#include "general/is_testnet.hpp"
#include "spdlog/spdlog.h"

struct TransferTxExchangeMessage;

namespace {
inline uint8_t* bewrite(uint8_t* pos, uint64_t val)
{
    val = hton64(val);
    memcpy(pos, &val, 8);
    return pos + 8;
}
inline uint8_t* bewrite(uint8_t* pos, uint32_t val)
{
    val = hton32(val);
    memcpy(pos, &val, 4);
    return pos + 4;
}
inline uint8_t* bewrite(uint8_t* pos, uint16_t val)
{
    val = hton16(val);
    memcpy(pos, &val, 2);
    return pos + 2;
}

class BlockGenerator_v2 {
    class NewAddressSection_v2 {
    public:
        NewAddressSection_v2(const ChainDB& db)
            : db(db)
            , nextStateId(db.next_state_id())
        {
        }

        std::optional<AccountId> getId(const AddressView address, bool allowNew)
        {
            if (auto iter = cache.find(address); iter != cache.end()) {
                return iter->second;
            }
            auto p = db.lookup_address(address);
            if (p) { // not present in database
                auto [id, _] = *p;
                cache.emplace(address, id);
                return id;
            } else {
                if (!allowNew)
                    return {};
                auto id = nextStateId++;
                auto iter = cache.emplace(address, id).first;
                newEntries.push_back(iter->first);
                assert(newEntries.size() < std::numeric_limits<uint16_t>::max());
                return id;
            }
        }
        void clear() { newEntries.clear(); }
        size_t binarysize() { return 2 + 20 * newEntries.size(); }
        uint8_t* write(uint8_t* out);

    private:
        const ChainDB& db;
        AccountId nextStateId;
        std::vector<AddressView> newEntries;
        std::map<Address, AccountId, Address::Comparator> cache;
    };

    class PaymentSection {
    public:
        PaymentSection(uint32_t n)
        {
            if (n == 0)
                buf.resize(0);
            else {
                buf.reserve(4 + n * 99);
                buf.resize(4);
            }
        }
        void add_payment(AccountId toId, PinNonce pinNonce,
            const TransferTxExchangeMessage&);
        size_t binarysize()
        {
            return buf.size();
        }
        uint8_t* write(uint8_t* out)
        {
            bewrite(buf.data(), nTransfers);
            memcpy(out, buf.data(), buf.size());
            return out + buf.size();
        }

    private:
        uint32_t nTransfers { 0 };
        std::vector<uint8_t> buf;
    };

    struct PayoutSection_v2 {
    public:
        PayoutSection_v2(AccountId toId, Funds amount)
        {
            buf.resize(16);
            uint8_t* pos = buf.data();
            pos = bewrite(pos, toId.value());
            pos = bewrite(pos, amount.E8());
        }
        size_t binarysize()
        {
            assert(buf.size() == 16);
            return buf.size();
        }
        uint8_t* write(uint8_t* out)
        {
            memcpy(out, buf.data(), buf.size());
            return out + buf.size();
        }

    private:
        std::vector<uint8_t> buf;
    };

public:
    BlockGenerator_v2(const ChainDB& db)
        : nas(db)
    {
    }
    BodyContainer gen_block_v2(NonzeroHeight height, const Payout& payouts,
        const std::vector<TransferTxExchangeMessage>& payments);

private:
    NewAddressSection_v2 nas;
};

BodyContainer BlockGenerator_v2::gen_block_v2(NonzeroHeight height,
    const Payout& payout,
    const std::vector<TransferTxExchangeMessage>& payments)
{
    nas.clear();

    // Payouts
    PayoutSection_v2 pos(*nas.getId(payout.to, true), payout.amount);

    // Payments
    if (payments.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Too many payments");
    }

    // filter valid payments to survive self send
    std::vector<TransferTxExchangeMessage> validPayments;
    for (auto& pmsg : payments) {
        if (nas.getId(pmsg.toAddr, true).value() == pmsg.from_id()) {
            // This should not be possible because self sending transactions
            // are detected on entering mempool.
            spdlog::warn("Impossible self send detected.");
            continue;
        }
        validPayments.push_back(pmsg);
    }

    PaymentSection pms(validPayments.size());
    for (auto& pmsg : validPayments) {
        size_t size { 10 + nas.binarysize() + pos.binarysize() + pms.binarysize() };
        assert(size <= MAXBLOCKSIZE);
        size_t remaining = MAXBLOCKSIZE - size;
        if (remaining < 99)
            break;
        bool allowNewAddress { remaining >= 99 + 20 };
        auto toId = nas.getId(pmsg.toAddr, allowNewAddress);
        if (!toId)
            break;

        auto ph = pmsg.pin_height();
        auto pn = PinNonce::make_pin_nonce(pmsg.nonce_id(), height, ph);
        if (!pn)
            throw std::runtime_error("Cannot make pin_nonce");

        pms.add_payment(*toId, *pn, pmsg);
    }

    // Serialize block
    size_t size { 10 + nas.binarysize() + pos.binarysize() + pms.binarysize() };
    if (size > MAXBLOCKSIZE) {
        throw std::runtime_error("Block size too large");
    }
    std::vector<uint8_t> out;
    out.resize(size);
    uint8_t* p = out.data() + 10;
    p = nas.write(p);
    p = pos.write(p);
    pms.write(p);
    return out;
}

uint8_t* BlockGenerator_v2::NewAddressSection_v2::write(uint8_t* out)
{
    uint16_t n = newEntries.size();
    out = bewrite(out, n);
    for (size_t i = 0; i < newEntries.size(); ++i) {
        memcpy(out, newEntries[i].data(), 20);
        out += 20;
    }
    return out;
}

void BlockGenerator_v2::PaymentSection::add_payment(
    AccountId toId, PinNonce pinNonce,
    const TransferTxExchangeMessage& m)
{
    nTransfers += 1;
    size_t offset = buf.size();
    buf.resize(offset + 99);
    uint8_t* pos = buf.data() + offset;
    Writer w(pos, 99);
    w << m.from_id() // 8
      << pinNonce // 8
      << m.compactFee // 2
      << toId // 8
      << m.amount // 8
      << m.signature; // 65
    assert(w.remaining() == 0);
}
}

BodyContainer generate_body(const ChainDB& db, NonzeroHeight height, const Payout& payout, const std::vector<TransferTxExchangeMessage>& payments)
{
    BlockGenerator_v2 bg(db);
    return bg.gen_block_v2(height, payout, payments);
}
