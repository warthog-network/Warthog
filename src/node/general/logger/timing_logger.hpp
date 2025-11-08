#pragma once
#include "spdlog/spdlog.h"
namespace logging {
struct TimingSession;
struct TimingObject {
private:
    TimingObject& operator=(const TimingObject&) = default;
    TimingObject& operator=(TimingObject&&) = default;

public:
    std::string name;
    size_t id;
    const TimingSession* session;
    std::chrono::steady_clock::time_point begin;
    TimingObject(TimingObject&& other)
    {
        *this = std::move(other);
        other.session = nullptr;
    };
    TimingObject(std::string name, size_t id, const TimingSession& logger)
        : name(std::move(name))
        , id(id)
        , session(&logger)
        , begin(std::chrono::steady_clock::now())
    {
    }
    ~TimingObject();
};

inline std::atomic<size_t> timingSessionCounter { 0 };

struct TimingSession {
    friend TimingObject;

private:
    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<spdlog::logger> _longrunningLogger;
    struct TimingEntry {
        std::string name;
        std::chrono::steady_clock::time_point begin;
        std::chrono::steady_clock::time_point end;
        auto ms() const
        {
            using namespace std::chrono;
            return duration_cast<milliseconds>(end - begin).count();
        }
    };
    std::chrono::steady_clock::time_point begin;
    mutable std::vector<TimingEntry> timings;

public:
    TimingSession(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<spdlog::logger> longrunningLogger)
        : _logger(std::move(logger))
        , _longrunningLogger(std::move(longrunningLogger))
        , begin(std::chrono::steady_clock::now())
    {
    }
    ~TimingSession()
    {
        using namespace std::chrono;
        auto d { steady_clock::now() - begin };
        auto ms { duration_cast<milliseconds>(d).count() };
        std::string s;
        bool first = true;
        for (auto& t : timings) {
            s += " " + t.name + " " + std::to_string(t.ms()) + "ms";
            if (!first) {
                s += ",";
            }
        }
        _logger->warn("Chainserver iteration took {} ms:{}", ms, s);
        if (ms >= 30) {
            _longrunningLogger->warn("Long running iteration took {} ms:{}", ms, s);
        }
    }
    TimingObject time(std::string s) const
    {
        return { std::move(s), timingSessionCounter.fetch_add(1), *this };
    }
};
struct TimingLogger {

private:
    mutable std::atomic<size_t> id { 0 };
    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<spdlog::logger> _longrunningLogger;

public:
    TimingLogger(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<spdlog::logger> longrunningLogger)
        : _logger(std::move(logger))
        , _longrunningLogger(std::move(longrunningLogger))
    {
    }
    TimingSession session() const
    {
        return { _logger, _longrunningLogger };
    }
};

inline TimingObject::~TimingObject()
{
    if (session)
        session->timings.push_back({ std::move(name), begin, std::chrono::steady_clock::now() });
}
}
