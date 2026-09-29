#include "tcc/server/config.hpp"

#include <charconv>
#include <limits>
#include <string>
#include <system_error>

namespace tcc::server {
namespace {

std::string require_value(
    const int argc,
    char* argv[],
    int& index,
    const std::string_view option) {
    if (index + 1 >= argc) {
        throw ConfigError("missing value for " + std::string(option));
    }

    ++index;
    std::string value(argv[index]);
    if (value.empty()) {
        throw ConfigError("empty value for " + std::string(option));
    }
    return value;
}

std::uint32_t parse_repetitions(const std::string& value) {
    std::uint64_t parsed = 0U;
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed == 0U ||
        parsed > std::numeric_limits<std::uint32_t>::max()) {
        throw ConfigError(
            "--repetitions must be an integer between 1 and 4294967295");
    }
    return static_cast<std::uint32_t>(parsed);
}

}  // namespace

ServerConfig parse_server_config(const int argc, char* argv[]) {
    if (argc < 1 || argv == nullptr) {
        throw ConfigError("invalid process arguments");
    }

    ServerConfig config;
    for (int index = 1; index < argc; ++index) {
        const std::string_view option(argv[index]);
        if (option == "--help" || option == "-h") {
            config.show_help = true;
        } else if (option == "--repetitions") {
            config.repetitions = parse_repetitions(
                require_value(argc, argv, index, option));
        } else if (option == "--client") {
            config.client = require_value(argc, argv, index, option);
        } else if (option == "--output") {
            config.output_path = require_value(argc, argv, index, option);
        } else {
            throw ConfigError("unknown option: " + std::string(option));
        }
    }
    return config;
}

std::string_view server_usage() noexcept {
    return
        "Usage: tcc_server [options]\n"
        "  --repetitions <count>  Repetitions per combination (default: 30)\n"
        "  --client <name>        Client label stored in the CSV\n"
        "  --output <path>        Measurement CSV (default: server_measurements.csv)\n"
        "  -h, --help             Show this help\n";
}

}  // namespace tcc::server
