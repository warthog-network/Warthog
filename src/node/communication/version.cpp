#include "version.hpp"
#include "spdlog/common.h"
#include <string>
namespace fmt_lib = spdlog::fmt_lib;

std::string NodeVersion::to_string() const
{
    return fmt_lib::format("v{}.{}.{}", major(), minor(), patch());
}
