#pragma once

#include "communication/rxtx_server/aggregator.hpp"
#include <map>
#include <string>
#include <vector>

namespace api {
struct TransmissionTimeseries {
    std::map<std::string, std::vector<rxtx::RangeAggregated>> byHost;
};
}
