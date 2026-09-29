#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tcc::server {

inline constexpr std::uint32_t default_repetitions = 30U;

struct ServerConfig {
    std::uint32_t repetitions{default_repetitions};
    std::string client{"unspecified"};
    std::string output_path{"server_measurements.csv"};
    bool show_help{false};
};

class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] ServerConfig parse_server_config(int argc, char* argv[]);
[[nodiscard]] std::string_view server_usage() noexcept;

}  // namespace tcc::server
