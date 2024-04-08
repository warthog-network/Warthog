#include "probe_balanced.hpp"
#include "../../types/probe_data.hpp"

namespace {
bool can_download(Height forkLower, Height forkUpper, Height bound)
{
    assert(forkLower <= forkUpper);
    assert(forkUpper <= bound + 1);
    auto delta = forkUpper - forkLower;
    auto downloadLength = (bound + 1 - forkLower);
    assert(downloadLength > 0); // otherwise the batch would be shared
    return (downloadLength < 20 || delta * 2 < downloadLength);
}
}

std::optional<Proberequest> ProbeBalanced::probe_request(const ProbeData& pd, const std::shared_ptr<Descripted>& desc, Height maxLength)
{
    return ProbeBalanced { pd, maxLength }.probe_request(desc);
}

[[nodiscard]] std::optional<Batchrequest> ProbeBalanced::slot_batch_request(const ProbeData& pd, const std::shared_ptr<Descripted>& desc, Batchslot slot, HashView h)
{
    auto maxLength = slot.upper();
    ProbeBalanced pb { pd, maxLength };
    auto l { pb.lower() };
    auto u { pb.upper() };

    if (can_download(l, u, maxLength)) {
        return Batchrequest(desc, pd.headers(), slot.lower(), maxLength, h);
    }
    return {};
}

[[nodiscard]] std::optional<Batchrequest> ProbeBalanced::final_partial_batch_request(const ProbeData& pd, const std::shared_ptr<Descripted>& desc, NonzeroHeight maxLength, Worksum minWork)
{
    Batchslot slot(maxLength);
    if (slot.upper() == maxLength)
        return {};
    ProbeBalanced pb { pd, maxLength };
    auto l { pb.lower() };
    auto u { pb.upper() };

    if (can_download(l, u, maxLength)) {
        if (slot.lower() > l) {
            return Batchrequest(desc, slot, maxLength - slot.offset(), minWork);
        } else {
            return Batchrequest(desc, pd.headers(), l, maxLength, minWork);
        }
    }
    return {};
}

NonzeroHeight ProbeBalanced::upper()
{
    return upper(probeData, maxLength);
}
NonzeroHeight ProbeBalanced::lower()
{
    return lower(probeData);
}

NonzeroHeight ProbeBalanced::lower(const ProbeData& probeData)
{
    return probeData.fork_range().lower();
}

NonzeroHeight ProbeBalanced::upper(const ProbeData& probeData, Height maxLength)
{
    assert(maxLength + 1 >= probeData.fork_range().lower());
    assert(probeData.headers()->length() + 1 >= probeData.fork_range().lower()); // TODO: this fails for some people, must be a bug somewhere
    auto u { (std::min(probeData.headers()->length(), maxLength) + 1).nonzero_assert() };
    if (auto& fr = probeData.fork_range(); fr.forked() && fr.upper() < u)
        u = fr.upper();
    return u;
}

std::optional<Proberequest> ProbeBalanced::probe_request(const std::shared_ptr<Descripted>& desc)
{
    auto l { lower() };
    auto u { upper() };

    if (!can_download(l, u, maxLength)) {
        auto h { l + (u - l) / 2 };
        return Proberequest(desc, h);
    }

    return {};
}
