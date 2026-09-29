#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace tcc::client {

struct Config {
    std::string server_ipv4;
    std::uint32_t repetitions{30};
    std::string client;
    std::string output_path;
};

// One control per run. cancel() may be called from another thread.
class Control final {
public:
    void cancel();
    [[nodiscard]] bool cancelled() const { return cancelled_.load(); }
    void attach(int descriptor);
    void detach();
private:
    std::atomic<bool> cancelled_{false};
    std::mutex mutex_;
    int descriptor_{-1};
};

struct Progress {
    std::uint64_t completed{};
    std::uint64_t total{};
    std::string detail;
};

struct Summary {
    std::uint64_t succeeded{};
    std::uint64_t failed{};
    bool cancelled{};
    std::string error;
};

// Stops on failure: the headerless protocol cannot resynchronize a lost connection.
[[nodiscard]] Summary run(const Config& config, Control& control,
                          const std::function<void(const Progress&)>& progress = {});

}  // namespace tcc::client
