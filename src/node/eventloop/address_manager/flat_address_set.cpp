// #include "flat_address_set.hpp"
// #include <algorithm>
//
// namespace address_manager {
// bool FlatAddressSet::contains(TCPSockaddr a)
// {
//     auto iter = std::lower_bound(vec.begin(),vec.end(), a);
//     return iter != vec.end() && *iter == a;
// }
// void FlatAddressSet::insert(TCPSockaddr& a)
// {
//     auto iter = std::lower_bound(vec.begin(),vec.end(), a);
//     if (iter == vec.end())
//         vec.insert(iter, a);
//     else if (*iter != a) {
//         vec.insert(std::next(iter), a);
//     }
// }
// void FlatAddressSet::erase(TCPSockaddr a)
// {
//     auto iter = std::lower_bound(vec.begin(),vec.end(), a);
//     if (iter != vec.end() && *iter == a) {
//         vec.erase(iter);
//     }
// };
// }
