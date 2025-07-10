#include "body.hpp"
#include "block/body/container.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/is_testnet.hpp"
#include "general/writer.hpp"
namespace block {
namespace body {
namespace elements {
void TokenSection::write(MerkleWriteHooker& w)
{
    w.writer << assetId;
    token_entries().write(w);
}
}

MerkleWriteHook::MerkleWriteHook(Writer& w, MerkleWriteHooker& c)
    : writer(w)
    , creator(c)
    , begin(w.cursor())
{
}

MerkleWriteHook::~MerkleWriteHook()
{
    if (begin)
        std::span<const uint8_t>(begin, writer.cursor());
}

MerkleWriteHook MerkleWriteHooker::hook()
{
    return { writer, *this };
}

std::vector<uint8_t> MerkleLeaves::merkle_prefix() const
{
    const std::vector<Hash>* from;
    std::vector<Hash> tmp, to;
    from = &hashes;

    do {
        const size_t I { (from->size() + 1) / 2 };
        to.clear();
        to.reserve(I);
        size_t j = 0;
        for (size_t i = 0; i < I; ++i) {
            if (I == 1) {
                std::vector<uint8_t> res;
                std::copy((*from)[j].begin(), (*from)[j].end(), std::back_inserter(res));
                if (j + 1 < from->size())
                    std::copy((*from)[j + 1].begin(), (*from)[j + 1].end(), std::back_inserter(res));
                return res;
            }
            HasherSHA256 hasher {};
            hasher.write((*from)[j]);
            if (j + 1 < from->size()) {
                hasher.write((*from)[j + 1]);
            }

            to.push_back(std::move(hasher));
            j += 2;
        }
        std::swap(tmp, to);
        from = &tmp;
    } while (from->size() > 1);
    assert(false);
}

Hash MerkleLeaves::merkle_root(const BodyData& data, NonzeroHeight h) const
{
    const std::vector<Hash>* from;
    std::vector<Hash> tmp, to;
    from = &hashes;

    bool new_root_type = is_testnet() || h.value() >= NEWMERKLEROOT;
    bool block_v2 = is_testnet() || h.value() >= NEWBLOCKSTRUCUTREHEIGHT;
    if (new_root_type) {
        do {
            const size_t I { (from->size() + 1) / 2 };
            to.clear();
            to.reserve(I);
            size_t j = 0;
            for (size_t i = 0; i < I; ++i) {
                HasherSHA256 hasher {};
                hasher.write((*from)[j]);
                if (j + 1 < from->size())
                    hasher.write((*from)[j + 1]);
                if (I == 1)
                    hasher.write({ data.data(), block_v2 ? 10u : 4u });
                to.push_back(std::move(hasher));
                j += 2;
            }
            std::swap(tmp, to);
            from = &tmp;
        } while (from->size() > 1);
        return from->front();
    } else {
        bool includedSeed = false;
        bool finish = false;
        do {
            const size_t I { (from->size() + 1) / 2 };
            to.clear();
            to.reserve(I);
            if (from->size() <= 2 && !includedSeed) {

                HasherSHA256 hasher {};
                hasher.write((*from)[0]);
                if (1 < from->size()) {
                    hasher.write((*from)[1]);
                }
                hasher.write({ data.data(), 4u });
                includedSeed = true;
                to.push_back(std::move(hasher));
            } else {
                if (from->size() == 1)
                    finish = true;
                size_t j = 0;
                for (size_t i = 0; i < (from->size() + 1) / 2; ++i) {
                    HasherSHA256 hasher {};
                    hasher.write((*from)[j]);
                    if (j + 1 < from->size()) {
                        hasher.write((*from)[j + 1]);
                    }
                    to.push_back(std::move(hasher));
                    j += 2;
                }
            }
            std::swap(tmp, to);
            from = &tmp;
        } while (!finish);
        return from->front();
    }
}

std::vector<TransactionId> ParsedBody::tx_ids(NonzeroHeight height) const
{
    auto pf { height.pin_floor() };
    std::vector<TransactionId> res;
    wart_transfers().append_txids(res, pf);
    entries().append_txids(res, pf);
    return res;
}

ParsedBody ParsedBody::parse_throw(std::span<const uint8_t> data, NonzeroHeight h, BlockVersion version, body::MerkleLeaves* ptree)
{
    if (data.size() > MAXBLOCKSIZE)
        throw Error(EINV_BODY);
    Reader r(data);
    body::MerkleReadHooker merkle(r, ptree);
    try {
        bool block_v2 = is_testnet() || h.value() >= NEWBLOCKSTRUCUTREHEIGHT;

        body_vector<Address> addresses;
        if (block_v2) {
            // Read new address section
            r.skip(10); // for mining
            addresses = { r.uint16(), merkle };
        } else {
            // Read new address section
            r.skip(4); // for mining

            addresses = { r.uint32(), merkle };
        }

        auto reward {
            [&]() {
                // create hook to auto-insert merkle entry
                auto hook { merkle.hook() };
                if (!block_v2)
                    r.skip(2); // # of entries, which should be 1
                return body::Reward { r };
            }()
        };
        body::Entries entries(merkle);
        entries.validate_version(version);
        return ParsedBody { std::move(addresses), std::move(reward), std::move(entries) };
    } catch (const Error& e) {
        if (e.code == EMSGINTEGRITY)
            throw Error(EINV_BODY); // more meaningful error
        throw e;
    }
}

size_t ParsedBody::byte_size() const
{
    size_t res { 0 };
    res += 10; // for mining
    res += 2 + newAddresses.byte_size();
    res += reward.byte_size();
    res += entries().byte_size();
    return res;
}

SerializedBody ParsedBody::serialize() const
{
    std::vector<uint8_t> res(byte_size(), 0);
    MerkleLeaves tree;
    Writer w(res);
    MerkleWriteHooker merkle(w, &tree);
    w.skip(10); // for mining
    newAddresses.write<uint16_t>(merkle);
    {
        auto h { merkle.hook() }; // hook merkle for reward
        w << reward;
    }
    entries().write(merkle);
    assert(w.remaining() == 0);
    return { VersionedBodyData {
                 std::move(res),
                 BlockVersion::v4,
             },
        std::move(tree) };
};

ParsedBody::ParsedBody(std::span<const uint8_t> data, BlockVersion v, NonzeroHeight h, body::MerkleLeaves* ptree)
    : ParsedBody(parse_throw(data, h, v, ptree)) {
    };

Body Body::parse_throw(VersionedBodyData c, NonzeroHeight h)
{
    auto p { c.parse(h, c.version) };
    return Body(std::move(p.first), std::move(c), std::move(p.second));
}
Body Body::serialize(ParsedBody b)
{
    auto ser { b.serialize() };
    return Body { std::move(b), std::move(ser.container), std::move(ser.merkleTree) };
}
}
}
