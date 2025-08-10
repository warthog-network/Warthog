#include "gui.hpp"

int main()
{
    auto gui { ui::GUI::create_instance() };
    bool shutdown = false;
    std::condition_variable cv;
    std::mutex m;
    std::thread t([&]() {
        bool b { false };
        while (true) {
            {
                std::unique_lock l(m);
                if (cv.wait_for(l, std::chrono::seconds(1),
                        [&]() { return shutdown == true; }))
                    break;
            }
            b = !b;
            gui->set_connected(b);
            gui->set_unlocked(!b);
        }
        gui->terminate();
    });
    gui->run();
    {
        std::unique_lock l(m);
        shutdown = true;
    }
    cv.notify_one();
    t.join();

    return 0;
}
