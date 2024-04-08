#pragma once
#include "../../types/peer_requests.hpp"
struct ProbeData;
struct ProbeBalanced {

    [[nodiscard]] static std::optional<Batchrequest> slot_batch_request(const ProbeData&, const std::shared_ptr<Descripted>&, Batchslot s, HashView h);
    [[nodiscard]] static std::optional<Batchrequest> final_partial_batch_request(const ProbeData&, const std::shared_ptr<Descripted>&, NonzeroHeight maxLength, Worksum minWork);
    [[nodiscard]] static std::optional<Proberequest> probe_request(const ProbeData&, const std::shared_ptr<Descripted>&, Height maxLength);

    [[nodiscard]] static NonzeroHeight lower(const ProbeData&);
    [[nodiscard]] static NonzeroHeight upper(const ProbeData&, Height maxLength);

    const ProbeData& probeData;
    Height maxLength;

private:
    [[nodiscard]] std::optional<Batchrequest> batch_request(const std::shared_ptr<Descripted>& desc, std::optional<Header>, Batchslot);
    [[nodiscard]] std::optional<Proberequest> probe_request(const std::shared_ptr<Descripted>& desc);

    [[nodiscard]] NonzeroHeight lower();
    [[nodiscard]] NonzeroHeight upper();
};
