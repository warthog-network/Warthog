#include "websocket_outbound_schedule.hpp"
using namespace std::chrono_literals;
#include "general/errors.hpp"

namespace connection_schedule {

WSConnectionSchedule::WSConnectionSchedule(InitArg ia)
{
    for (auto &addr : ia.pin) {
        insert(addr);
    }
}

void WSConnectionSchedule::set_element_expiration(Element& elem, std::chrono::steady_clock::duration d)
{
    if (d > 5min)
        d = 5min;
    auto expires { std::chrono::steady_clock::now() + d };
    elem.request.sleptFor = d;
    elem.expires = expires;
    wakeup_tp.consider(expires);
}

void WSConnectionSchedule::connect_expired(time_point now)
{
    if (!wakeup_tp.expired(now))
        return;
    wakeup_tp.reset();
    for (auto& r : pinnedRequests) {
        if (r.expires.has_value()) {
            if (*r.expires <= now) { // expired
                r.expires.reset();
                r.request.connect();
            } else
                wakeup_tp.consider(*r.expires);
        }
    }
}

void WSConnectionSchedule::outbound_closed(const WSBrowserConnectRequest& r, bool success, int32_t reason)
{
    if (auto iter { find_element(r.address()) }; iter != pinnedRequests.end()) {
        auto& elem { *iter };
        if (!elem.expires) {
            if (errors::leads_to_ban(reason)) {
                set_element_expiration(elem, 5min);
            } else {
                if (!success)
                    set_element_expiration(elem, r.sleptFor + 20s);
                else {
                    elem.request.sleptFor = 0s;
                    elem.request.connect();
                }
            }
        }
    }
}

void WSConnectionSchedule::outbound_failed(const WSBrowserConnectRequest& r)
{
    if (auto iter { find_element(r.address()) }; iter != pinnedRequests.end()) {
        auto& elem { *iter };
        if (!elem.expires) {
            set_element_expiration(elem, r.sleptFor + 20s);
        }
    }
}

auto WSConnectionSchedule::find_element(const WSUrladdr& addr) -> vector_t::iterator
{
    return std::ranges::find_if(pinnedRequests, [&addr](const Element& e) {
        return e.request.address() == addr;
    });
}

void WSConnectionSchedule::insert(const WSUrladdr& addr)
{
    if (auto iter { find_element(addr) }; iter == pinnedRequests.end()) {
        pinnedRequests.push_back(WSBrowserConnectRequest::make_outbound(addr));
        pinnedRequests.back().request.connect();
    }
}

bool WSConnectionSchedule::erase(const WSUrladdr& addr)
{
    if (auto iter { find_element(addr) }; iter != pinnedRequests.end()) {
        pinnedRequests.erase(iter);
        return true;
    }
    return false;
}

auto WSConnectionSchedule::pop_wakeup_time() -> std::optional<time_point>
{
    return wakeup_tp.pop();
}

}
