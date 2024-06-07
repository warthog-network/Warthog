#pragma once
#include "address_manager.hpp"
#include "eventloop/types/conndata_impl.hpp"

inline void AddressManager::All::Iterator::find_next()
{
    while (true) {
        if (iter == ref.conndatamap.end() || !iter->second.erased())
            break;
        ++iter;
    }
}
inline void AddressManager::Initialized::Iterator::find_next()
{
    while (true) {
        if (iter == ref.conndatamap.end() || ((!iter->second.erased()) && (*iter).initialized()))
            break;
        ++iter;
    }
}
