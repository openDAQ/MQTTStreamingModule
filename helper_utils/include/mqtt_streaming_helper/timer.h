#pragma once

#include <chrono>

namespace helper::utils
{
class Timer
{
public:
    Timer(size_t ms, bool start = true)
        : period(ms),
          firstStart(true)
    {
        if (start)
            restart();
    }

    std::chrono::milliseconds remain() const
    {
        auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        std::chrono::milliseconds newTout = (elapsed_ms >= timeout) ? std::chrono::milliseconds(0) : timeout - elapsed_ms;
        return newTout;
    }

    bool expired()
    {
        return (firstStart) ? true : (remain() == std::chrono::milliseconds(0));
    }

    explicit operator std::chrono::milliseconds() const noexcept
    {
        return remain();
    }

    void restart()
    {
        firstStart = false;
        start = std::chrono::steady_clock::now();
        timeout = std::chrono::milliseconds(period);
    }

protected:
    std::chrono::steady_clock::time_point start;
    std::chrono::milliseconds timeout;
    std::chrono::milliseconds period;
    bool firstStart;
};

} // namespace helper::utils
