#pragma once

#include <chrono>
#include <cstdint>

namespace tcc::benchmark {

class Timer final {
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    [[nodiscard]] static TimePoint now() noexcept;
    [[nodiscard]] static std::int64_t elapsed_nanoseconds(
        TimePoint start,
        TimePoint end);
};

static_assert(Timer::Clock::is_steady,
              "benchmark timing requires a monotonic clock");

}  // namespace tcc::benchmark
