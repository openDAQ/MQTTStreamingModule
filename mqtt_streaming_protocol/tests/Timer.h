#pragma once

#include <chrono>

class Timer
{
public:
    Timer(int ms)
    {
        start = std::chrono::steady_clock::now();
        timeout = std::chrono::milliseconds(ms);
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
        return remain() == std::chrono::milliseconds(0);
    }
    explicit operator std::chrono::milliseconds() const noexcept
    {
        return remain();
    }

protected:
    std::chrono::steady_clock::time_point start;
    std::chrono::milliseconds timeout;
};
