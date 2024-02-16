#pragma once
#include "block/body/account_id.hpp"
#include "crypto/address.hpp"
#include<variant>
namespace API{
struct AccountIdOrAddress{
    std::variant<AccountId,Address> data;
    auto visit(const auto& lambda){
        return std::visit(lambda,data);
    }
};
}
