#include <list>
#include <memory>

class RTCConnection;
namespace rtc_state {
using registry_t = std::list<std::shared_ptr<RTCConnection>>;
}
