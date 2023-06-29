#include "batch.hpp"
#include "block/header/header_impl.hpp"
#include "general/now.hpp"
#include "timestamprule.hpp"

Worksum Batch::worksum(const Height offset, uint32_t maxElements) const
{ // OK
    const uint32_t s = std::min((uint32_t)size(), maxElements);
    if (s == 0)
        return {};
    uint32_t rel_upper{s - 1};
    Worksum sum;
    bool complete = false;
    while (!complete) {
        auto header = get_header(rel_upper);
        assert(header);
        Worksum w(header->target());
        Height rf = (offset + rel_upper).retarget_floor();
        uint32_t factor;
        if (rf == Height(1)) {
            complete = true;
            factor = rel_upper + 1;
        } else {
            if (rf <= offset) {
                complete = true;
                factor = rel_upper + 1;
            } else {
                uint32_t rel_lower = rf - (offset + 1);
                factor = rel_upper - rel_lower;
                assert(factor > 0);
                rel_upper = rel_lower;
            }
        }
        w *= factor;
        sum += w;
    }
    return sum;
}


bool Batch::valid_inner_links()
{
    if (size() <= 1)
        return true;
    const size_t N = size();
    size_t i = 0;
    for (i = 1; i < N; ++i) {
        if (operator[](i - 1).hash() != operator[](i).prevhash())
            return false;
    }
    return true;
};


Grid::Grid(std::span<const uint8_t> s)
{
    if (s.size() % 80 != 0)
        throw Error(EMALFORMED);
    assign(s.begin().base(), s.end().base());
};

bool Grid::valid_checkpoint() const
{
    auto cp = GridPin::checkpoint();
    return (!cp)
        || (cp->slot < slot_end() && cp->finalHeader == operator[](cp->slot));
};
