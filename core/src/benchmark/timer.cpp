#include "tcc/benchmark/timer.hpp"

#include <stdexcept>

namespace tcc::benchmark {

Timer::TimePoint Timer::now() noexcept {
    return Clock::now();
}

std::int64_t Timer::elapsed_nanoseconds(const TimePoint start,
                                        const TimePoint end) {
    if (end < start) {
        throw std::logic_error("monotonic clock moved backwards");
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
        .count();
}

}  // namespace tcc::benchmark
