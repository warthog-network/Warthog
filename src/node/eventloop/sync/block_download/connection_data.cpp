#include "connection_data.hpp"
#include "eventloop/types/conref_impl.hpp"
#include "eventloop/types/conref_declaration.hpp"
#include "block/chain/state.hpp"
#include "focus.hpp"
class Focus;
namespace BlockDownload{

[[nodiscard]] ConnectionData& data(Conref cr){
    return cr->usage.data_blockdownload;
};

}
