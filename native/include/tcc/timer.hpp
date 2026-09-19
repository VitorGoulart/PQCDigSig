#pragma once

#include <chrono>
#include <cstdint>

namespace tcc {

class Timer {
public:
    using Clock = std::chrono::steady_clock;

    Timer() : started_at_(Clock::now()) {}

    [[nodiscard]] std::int64_t elapsed_nanoseconds() const noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   Clock::now() - started_at_)
            .count();
    }

private:
    Clock::time_point started_at_;
};

}  // namespace tcc
