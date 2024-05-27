
#include "rtc/rtc.hpp"
#include "transport/webrtc/sdp_util.hpp"
#include "transport/helpers/ip.hpp"

#include "transport/helpers/ipv4.hpp"
#include "transport/webrtc/connection.hxx"
#include "transport/helpers/ipv6.hpp"
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


int main()
{
    RTCConnection::fetch_id([](const std::vector<IP>& ips) {
        for (auto& ip : ips) {
            std::cout << "ip: " << ip.to_string() << endl;
        }
    }, false);

    sleep_for(1000s);
    return 0;
    try {
        // rtc::InitLogger(rtc::LogLevel::Debug);
        rtc::Configuration config {
            .iceServers {
                // { "stun:stun.l.google.com:19302" }
            }
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
