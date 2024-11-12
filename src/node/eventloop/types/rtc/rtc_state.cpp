#include "rtc_state.hpp"
#include "eventloop/types/conndata.hpp"
#include "rtc_state.hpp"
#include "general/logging.hpp"

namespace rtc_state {
std::optional<IP> RTCState::get_ip(IpType t) const
{
    using res_t = std::optional<IP>;
    if (!ips)
        return res_t {};
    switch (t) {
    case IpType::v4: {
        auto& v { ips->get_ip4() };
        if (v)
            return *v;
        break;
    }
    case IpType::v6: {
        auto& v { ips->get_ip6() };
        if (v)
            return *v;
        break;
    }
    }
    return {};
}

void VerificationSchedule::add(Conref c)
{
    auto& rtc { c.rtc() };
    if (rtc.verificationScheduled || rtc.their.identity.empty() || rtc.our.pendingVerification.has_value())
        return;
    rtc.verificationScheduled = true;
    queue.push_back(c);
}

void VerificationSchedule::erase(Conref c)
{
    size_t i = 0;
    std::erase_if(queue, [offset = this->offset, &i, &c](Conref& c2) -> bool {
        if (i < offset) {
            i += 1;
            return true;
        }
        return c == c2;
    });
    c.rtc().verificationScheduled = false;
    offset = 0;
}

std::optional<Conref> VerificationSchedule::pop_front()
{
    if (offset >= queue.size())
        return {};
    Conref res { queue[offset] };
    res.rtc().verificationScheduled = false;
    offset += 1;
    if (offset >= pruneOffset){
        queue.erase(queue.begin(), queue.begin() + offset);
        offset = 0;
    }
    return res;
}

auto VerificationSchedule::pop(IdentityIps::Pattern p) -> std::optional<PopResult>
{
    while (auto o { pop_front() }) {
        auto& c { *o };
        if (auto ip { c.rtc().their.identity.pop_unverified(p) }) {
            return PopResult { c, *ip };
        }
    }
    return {};
}

void Connections::insert(std::shared_ptr<RTCConnection> c)
{
    entries.push_back(std::move(c));
    auto iter { std::prev(entries.end()) };
    (*iter)->rtcRegistryIter = iter;
    log_rtc("Inserted RTC connection, ({} active)", size());
}

void Connections::erase(std::shared_ptr<RTCConnection>& c)
{
    entries.erase(c->rtcRegistryIter);
    log_rtc("Closed RTC connection, ({} active)", size());
}
}

void RTCState::erase(Conref c)
{
    verificationSchedule.erase(c);
}
