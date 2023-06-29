#include "generator.hpp"
#include "spdlog/spdlog.h"
#include "db/chain_db.hpp"

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
class BlockGenerator {
    class NewAddressSection {
    public:
        NewAddressSection(const ChainDB& db)
            : db(db)
            , nextStateId(db.next_state_id())
        {
        }

        AccountId getId(const AddressView address)
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
                auto id = nextStateId++;
                auto iter = cache.emplace(address, id).first;
                newEntries.push_back(iter->first);
                return id;
            }
        }
        void clear() { newEntries.clear(); }
        size_t binarysize() { return 4 + 20 * newEntries.size(); }
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
            buf.reserve(4 + n * 99);
            buf.resize(4);
            bewrite(buf.data(), n);
        }
        void add_payment(AccountId toId, PinNonce pinNonce,
            const TransferTxExchangeMessage&);
        size_t binarysize() { return buf.size(); }
        uint8_t* write(uint8_t* out)
        {
            memcpy(out, buf.data(), buf.size());
            return out + buf.size();
        }

    private:
        std::vector<uint8_t> buf;
    };
    struct PayoutSection {
    public:
        PayoutSection(uint16_t n)
        {
            buf.reserve(2 + n * 16);
            buf.resize(2);
            bewrite(buf.data(), n);
        }
        void write(AccountId toId, Funds amount)
        {
            size_t offset = buf.size();
            buf.resize(offset + 16);
            uint8_t* pos = buf.data() + offset;
            pos = bewrite(pos, toId.value());
            pos = bewrite(pos, amount.E8());
        }
        size_t binarysize() { return buf.size(); }
        uint8_t* write(uint8_t* out)
        {
            memcpy(out, buf.data(), buf.size());
            return out + buf.size();
        }

    private:
        std::vector<uint8_t> buf;
    };

public:
    BlockGenerator(const ChainDB& db)
        : nas(db)
    {
    }
    BodyContainer gen_block(Height height, const std::vector<Payout>& payouts,
        const std::vector<TransferTxExchangeMessage>& payments);

private:
    NewAddressSection nas;
};

BodyContainer BlockGenerator::gen_block(Height height,
    const std::vector<Payout>& payouts,
    const std::vector<TransferTxExchangeMessage>& payments
    )
{
    nas.clear();

    // Payouts
    if (payouts.size() > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("Too many payouts");
    }
    PayoutSection pos(payouts.size());
    for (auto& p : payouts) {
        AccountId toId = nas.getId(p.to);
        pos.write(toId, p.amount);
    }

    // Payments
    if (payouts.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Too many payments");
    }
    PaymentSection pms(payments.size());
    for (auto& pmsg : payments) {
        AccountId toId = nas.getId(pmsg.toAddr);

        auto ph = pmsg.pin_height();
        auto pn = PinNonce::make_pin_nonce(pmsg.nonce_id(), height, ph);
        if (!pn)
            throw std::runtime_error("Cannot make pin_nonce");

        pms.add_payment(toId, *pn, pmsg);
    }

    // Serialize block
    size_t size { 4 + nas.binarysize() + pos.binarysize() + pms.binarysize() };
    if (size > MAXBLOCKSIZE) {
        throw std::runtime_error("Block size too large");
    }
    std::vector<uint8_t> out;
    out.resize(size);
    uint8_t* p = out.data() + 4;
    p = nas.write(p);
    p = pos.write(p);
    pms.write(p);
    return out;
}

uint8_t* BlockGenerator::NewAddressSection::write(uint8_t* out)
{
    uint32_t n = newEntries.size();
    out = bewrite(out, n);
    for (size_t i = 0; i < newEntries.size(); ++i) {
        memcpy(out, newEntries[i].data(), 20);
        out += 20;
    }
    return out;
}

void BlockGenerator::PaymentSection::add_payment(
    AccountId toId, PinNonce pinNonce,
    const TransferTxExchangeMessage& m)
{
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

BodyContainer generate_body(const ChainDB& db, Height height, const std::vector<Payout>& payouts, const std::vector<TransferTxExchangeMessage>& payments){
    BlockGenerator bg(db);
    return bg.gen_block(height,payouts,payments);
};
