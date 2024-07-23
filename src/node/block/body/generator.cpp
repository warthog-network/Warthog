#include "generator.hpp"
#include "chainserver/db/chain_db.hpp"
#include "general/is_testnet.hpp"
#include "general/writer.hpp"
#include "spdlog/spdlog.h"

class TransferTxExchangeMessage;

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

class BlockGenerator {
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

    class TransferSection {
    public:
        void add_payment(AccountId toId, PinNonce pinNonce,
            const TransferTxExchangeMessage&);
        size_t binarysize()
        {
            if (buf.size() == 0) 
                return 0;
            return 4 + buf.size();
        }
        uint8_t* write(uint8_t* out)
        {
            if (buf.size() == 0) 
                return out;
            out = bewrite(out, nTransfers);
            memcpy(out, buf.data(), buf.size());
            return out + buf.size();
        }
        auto total_fee() const { return totalFee; }

    private:
        Funds totalFee { Funds::zero() };
        uint32_t nTransfers { 0 };
        std::vector<uint8_t> buf;
    };

    struct RewardSection {
    public:
        RewardSection(AccountId toId, Funds amount)
        {
            uint8_t* pos = buf.data();
            pos = bewrite(pos, toId.value());
            pos = bewrite(pos, amount.E8());
        }
        static constexpr size_t binary_size { 16 };
        uint8_t* write(uint8_t* out)
        {
            memcpy(out, buf.data(), buf.size());
            return out + buf.size();
        }

    private:
        std::array<uint8_t,16> buf;
    };

public:
    BlockGenerator(const ChainDB& db)
        : nas(db)
    {
    }
    BodyContainer gen_block(NonzeroHeight height, const Address& miner,
        const std::vector<TransferTxExchangeMessage>& payments);

private:
    NewAddressSection_v2 nas;
};

BodyContainer BlockGenerator::gen_block(NonzeroHeight height,
    const Address& miner,
    const std::vector<TransferTxExchangeMessage>& transfers)
{
    nas.clear();

    // Transfers
    if (transfers.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Too many payments");
    }

    // filter valid payments to survive self send
    std::vector<TransferTxExchangeMessage> validTransfers;
    for (auto& pmsg : transfers) {
        if (nas.getId(pmsg.toAddr, true).value() == pmsg.from_id()) {
            // This should not be possible because self sending transactions
            // are detected on entering mempool.
            spdlog::warn("Impossible self send detected.");
            continue;
        }
        validTransfers.push_back(pmsg);
    }

    TransferSection trs;
    for (auto& pmsg : validTransfers) {
        size_t size { 10 + nas.binarysize() + RewardSection::binary_size + trs.binarysize() };
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

        trs.add_payment(*toId, *pn, pmsg);
    }

    // Reward Section
    auto totalReward { Funds::sum_assert(height.reward(), trs.total_fee()) };
    RewardSection pos(*nas.getId(miner, true), totalReward);

    // Serialize block
    size_t size { 10 + nas.binarysize() + RewardSection::binary_size + trs.binarysize() };
    if (size > MAXBLOCKSIZE) {
        throw std::runtime_error("Block size too large");
    }
    std::vector<uint8_t> out;
    out.resize(size);
    uint8_t* p = out.data() + 10;
    p = nas.write(p);
    p = pos.write(p);
    trs.write(p);
    return out;
}

uint8_t* BlockGenerator::NewAddressSection_v2::write(uint8_t* out)
{
    uint16_t n = newEntries.size();
    out = bewrite(out, n);
    for (size_t i = 0; i < newEntries.size(); ++i) {
        memcpy(out, newEntries[i].data(), 20);
        out += 20;
    }
    return out;
}

void BlockGenerator::TransferSection::add_payment(
    AccountId toId, PinNonce pinNonce,
    const TransferTxExchangeMessage& m)
{
    nTransfers += 1;
    totalFee.add_assert(m.fee());
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

BodyContainer generate_body(const ChainDB& db, NonzeroHeight height, const Address& miner, const std::vector<TransferTxExchangeMessage>& payments)
{
    BlockGenerator bg(db);
    auto body{bg.gen_block(height, miner, payments)};
    BodyView bv{body.view(height)};
    // should be valid
    if (bv.valid()) 
        return body;
    
    // disable transactions as fallback
    return BlockGenerator(db).gen_block(height, miner, {});
}
