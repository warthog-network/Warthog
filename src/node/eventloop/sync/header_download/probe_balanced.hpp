#pragma once
#include "../../types/peer_requests.hpp"
struct ProbeData;
struct ProbeBalanced {

    [[nodiscard]] static wrt::optional<HeaderRequest> slot_batch_request(const ProbeData&, const std::shared_ptr<Descripted>&, Batchslot s, Header h);
    [[nodiscard]] static wrt::optional<HeaderRequest> final_partial_batch_request(const ProbeData&, const std::shared_ptr<Descripted>&, NonzeroHeight maxLength, Worksum minWork);
    [[nodiscard]] static wrt::optional<Proberequest> probe_request(const ProbeData&, const std::shared_ptr<Descripted>&, Height maxLength);

    [[nodiscard]] static NonzeroHeight lower(const ProbeData&);
    [[nodiscard]] static NonzeroHeight upper(const ProbeData&, Height maxLength);

    const ProbeData& probeData;
    Height maxLength;

private:
    [[nodiscard]] wrt::optional<HeaderRequest> batch_request(const std::shared_ptr<Descripted>& desc, wrt::optional<Header>, Batchslot);
    [[nodiscard]] wrt::optional<Proberequest> probe_request(const std::shared_ptr<Descripted>& desc);

    [[nodiscard]] NonzeroHeight lower();
    [[nodiscard]] NonzeroHeight upper();
};
