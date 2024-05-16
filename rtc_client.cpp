
#include "rtc/rtc.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <strstream>
#include <utility>

#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
typedef int SOCKET;

using nlohmann::json;
using std::this_thread::sleep_for;
using namespace std::chrono_literals;

using namespace std;

struct IdFetcher {
    static auto udp_candidate_ip(std::string_view s)
    {
        struct Result {
            bool candidate { false };
            std::optional<std::string_view> udp_ip;
        };
        if (!s.starts_with("a=candidate:"))
            return Result {};
        size_t i = 11;
        const size_t N { s.size() };
        size_t spaces = 0;
        bool prevspace = false;
        for (i = 11; i < N; ++i) {
            auto c { s[i] };
            if (c == ' ') {
                if (prevspace == false) {
                    spaces += 1;
                    prevspace = true;
                }
            } else {
                if (prevspace == true) {
                    if (spaces == 2) {
                        if (!s.substr(i).starts_with("UDP "))
                            return Result { true, {} };
                    } else if (spaces == 4) {
                        auto sub { s.substr(i) };
                        if (auto n { sub.find(' ') }; n == std::string::npos) {
                            return Result { true, {} };
                        } else {
                            return Result { true, sub.substr(0, n) };
                        }
                    }
                }
                prevspace = false;
            }
        }
        return Result { true, {} };
    }

    template <typename T>
    requires std::is_invocable_r_v<void, T, std::string_view>
    static void foreach_line(std::string_view sdp, T&& callback)
    {
        size_t i0 { 0 };
        for (size_t i = 0; i < sdp.size(); ++i) {
            auto c { sdp[i] };
            if (c == '\n') {
                auto line { sdp.substr(i0, i + 1 - i0) };
                callback(line);
                i0 = i + 1;
            }
        }
        if (i0 < sdp.size()) {
            auto line { sdp.substr(i0) };
            callback(line);
        }
    }

    static std::vector<std::string> extract_ips(std::string_view s)
    {
        std::vector<std::string> out;
        foreach_line(s, [&out](std::string_view line) {
            auto c { udp_candidate_ip(line) };
            if (auto ip { c.udp_ip })
                out.push_back(std::string(*ip));
        });
        return out;
    }

    static std::string filter_sdp(std::string_view s)
    {
        std::string out;
        out.reserve(s.size());
        foreach_line(s, [&out](std::string_view line) {
            auto c { udp_candidate_ip(line) };
            if (!c.candidate || c.udp_ip) {
                out += line;
            }
        });
        return out;
    }

    // template <typename T>
    // requires std::is_invocable_r_v<void, T, std::string_view>
    // static void foreach_udp_ip(std::string_view sdp, T&& callback)
    // {
    //     size_t i0 { 0 };
    //     for (size_t i = 0; i < sdp.size(); ++i) {
    //         auto c { sdp[i] };
    //         if (c == '\n') {
    //             auto line { sdp.substr(i0, i - i0 + 1) };
    //             callback(line);
    //             i0 = i + 1;
    //         }
    //     }
    //     //
    //     // std::istringstream is(std::move(sdp));
    //     // std::string line;
    //     // while (std::getline(is, line)) {
    //     //     if (auto ip { udp_candidate_ip(line) }; ip) {
    //     //         callback(std::string(*ip));
    //     //     }
    //     // }
    // }
    static auto default_config()
    {
        return rtc::Configuration {
            .iceServers {
                { "stun:stun.l.google.com:19302" } }
        };
    }

    bool start()
    {
        std::lock_guard l(m);
        if (auto u { p.lock() }; u)
            return false;

        rtc::Configuration config {
            .iceServers {
                { "stun:stun.l.google.com:19302" } }
        };

        auto pc = std::make_shared<rtc::PeerConnection>(config);

        pc->onGatheringStateChange(
            [pc, this](rtc::PeerConnection::GatheringState state) mutable {
                std::cout << "Gathering State: " << state << std::endl;
                if (state == rtc::PeerConnection::GatheringState::Complete) {
                    auto description = pc->localDescription();
                    // description.value().generateSdp()
                    // std::to_string
                    std::string sdp { description.value() };
                    json message = { { "type", description->typeString() },
                        { "sdp", sdp } };
                    std::string strmsg { message.dump(0) };
                    // std::cout << "Paset this offer into other peer: \n"
                    //           << strmsg << std::endl;
                    cout << "BEFORE " << sdp << endl;
                    cout << "AFTER " << filter_sdp(sdp) << endl;
                    for (auto ip : extract_ips(sdp)) {
                        cout << "IP " << ip << endl;
                    }
                    on_result(strmsg);
                }
            });
        auto dc { pc->createDataChannel("channel2") };
        return true;
    }

    IdFetcher(std::function<void(std::string)> on_result)
        : on_result(std::move(on_result))
    {
    }

private:
    std::mutex m;
    std::weak_ptr<rtc::PeerConnection> p;
    std::function<void(const std::string&)> on_result;
};

int main()
{
    IdFetcher pc([](const std::string& s) { std::cout << "on_result: " << s << endl; });
    pc.start();

    sleep_for(1000s);
    return 0;
    try {
        // rtc::InitLogger(rtc::LogLevel::Debug);
        rtc::Configuration config {
            .iceServers {
                { "stun:stun.l.google.com:19302" } }
        };

        auto pc = std::make_shared<rtc::PeerConnection>(config);

        pc->onLocalCandidate([](rtc::Candidate candidate) {
            std::cout << "Candidate " << candidate.candidate() << std::endl;
        });
        pc->onStateChange([](rtc::PeerConnection::State state) {
            std::cout << "State: " << state << std::endl;
        });

        std::promise<void> p;
        auto f { p.get_future() };

        pc->onGatheringStateChange(
            [pc, &p](rtc::PeerConnection::GatheringState state) mutable {
                std::cout << "Gathering State: " << state << std::endl;
                if (state == rtc::PeerConnection::GatheringState::Complete) {
                    auto description = pc->localDescription();
                    json message = { { "type", description->typeString() },
                        { "sdp", std::string(description.value()) } };
                    std::cout << "Paset this offer into other peer: \n"
                              << message << std::endl;
                    p.set_value();
                }
            });

        auto dc { pc->createDataChannel("channel1") };

        f.get();
        std::string reply;
        cout << "Paste other peer's respoinse here:" << endl;
        std::getline(cin, reply);
        auto parsed = json::parse(reply);
        auto sdp { parsed["sdp"].get<string>() };
        pc->setRemoteDescription({ sdp, parsed["type"].get<std::string>() });
        dc->onOpen([]() { cout << "OPEN" << endl; });
        dc->onMessage([](rtc::message_variant msg) {
            cout << "received a message: " << std::get<string>(msg) << endl;
        });
        dc->onClosed([]() { cout << "CLOSED" << endl; });
        dc->onError([](std::string error) { cout << "ERROR: " << error << endl; });

        sleep_for(1000s);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
