#include "rtc/rtc.hpp"
#include <iostream>
#include <memory>
#include <utility>
#include <nlohmann/json.hpp>
#include <chrono>
#include <future>

typedef int SOCKET;

using nlohmann::json;
using std::this_thread::sleep_for;
using namespace std::chrono_literals;
using namespace std;

int main()
{
    try {
        // rtc::InitLogger(rtc::LogLevel::Debug);
        rtc::Configuration config;
        config.iceServers.push_back({ "stun:stun.l.google.com:19302" });
        auto pc = std::make_shared<rtc::PeerConnection>(config);

        pc->onLocalCandidate([](rtc::Candidate candidate) {
            std::cout << "Candidate " << candidate.candidate() << std::endl;
        });
        pc->onStateChange([](rtc::PeerConnection::State state) {
            std::cout << "State: " << static_cast<int>(state) << std::endl;
            // std::cout << "State: " << state << std::endl;
        });

        std::promise<void> p;
        auto f { p.get_future() };

        pc->onGatheringStateChange(
            [pc, &p](rtc::PeerConnection::GatheringState state) mutable {
                // std::cout << "Gathering State: " << state << std::endl;
                if (state == rtc::PeerConnection::GatheringState::Complete) {
                    // auto description = pc->localDescription();
                    // json message = {{"type", description->typeString()},
                    //                 {"sdp", std::string(description.value())}};
                    // std::cout << "Message: " << message << std::endl;
                    p.set_value();
                }
            });
        pc->onDataChannel([](std::shared_ptr<rtc::DataChannel> dc) {
            std::cout << "New data channel" << endl;

            dc->onOpen([dc]() {
                cout << "OPEN" << endl;
                dc->send(std::string("Hello world!"));
            });
            dc->onMessage([dc](rtc::message_variant msg) {
                cout << "received a message: " << std::get<string>(msg) << endl;
            });
            dc->onClosed([]() { cout << "CLOSED" << endl; });
            dc->onError(
                [](std::string error) { cout << "ERROR: " << error << endl; });
        });

        std::string reply;
        cout << "Paste client's offer here: " << endl;
        std::getline(cin, reply);
        cout << "Got line." << endl;
        auto parsed = json::parse(reply);
        auto sdp { parsed["sdp"].get<string>() };
        pc->setRemoteDescription({ sdp, parsed["type"].get<std::string>() });
        f.get();
        auto description = pc->localDescription();
        json message = { { "type", description->typeString() },
            { "sdp", std::string(description.value()) } };
        std::cout << "Answer for client to connect:\n"
                  << message << std::endl;

        sleep_for(1000s);
        std::this_thread::sleep_for(std::chrono::seconds(1000));

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
