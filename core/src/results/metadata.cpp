#include "tcc/results/metadata.hpp"

#include <openssl/opensslv.h>
#include <oqs/oqsconfig.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "tcc/benchmark/payload_generator.hpp"
#include "tcc/benchmark/run_config.hpp"
#include "tcc/protocol/protocol_limits.hpp"
#include "tcc/results/result_writer.hpp"

namespace tcc::results {
namespace {

#define TCC_STRINGIFY_IMPL(value) #value
#define TCC_STRINGIFY(value) TCC_STRINGIFY_IMPL(value)

[[nodiscard]] std::string compiler_version() {
#if defined(__clang__)
    return std::string("Clang ") + TCC_STRINGIFY(__clang_major__) + "." +
           TCC_STRINGIFY(__clang_minor__) + "." +
           TCC_STRINGIFY(__clang_patchlevel__);
#elif defined(__GNUC__)
    return std::string("GCC ") + TCC_STRINGIFY(__GNUC__) + "." +
           TCC_STRINGIFY(__GNUC_MINOR__) + "." +
           TCC_STRINGIFY(__GNUC_PATCHLEVEL__);
#else
    return "unknown";
#endif
}

[[nodiscard]] bool is_supported_payload(
    const std::uint32_t payload_bytes) noexcept {
    return payload_bytes == protocol::limits::payload_10_kib ||
           payload_bytes == protocol::limits::payload_100_kib ||
           payload_bytes == protocol::limits::payload_1_mib;
}

[[nodiscard]] std::string_view status_name(const RunStatus status) noexcept {
    switch (status) {
        case RunStatus::completed:
            return "completed";
        case RunStatus::aborted:
            return "aborted";
    }
    return {};
}

[[nodiscard]] std::string_view expected_provider(
    const protocol::AlgorithmId algorithm) noexcept {
    switch (algorithm) {
        case protocol::AlgorithmId::ml_dsa_44:
        case protocol::AlgorithmId::ml_dsa_65:
        case protocol::AlgorithmId::ml_dsa_87:
        case protocol::AlgorithmId::rsa_pss:
        case protocol::AlgorithmId::ed25519:
            return "OpenSSL";
        case protocol::AlgorithmId::falcon_512:
        case protocol::AlgorithmId::falcon_1024:
            return "liboqs";
        case protocol::AlgorithmId::invalid:
            return {};
    }
    return {};
}

[[nodiscard]] bool is_ascii_digit(const char value) noexcept {
    return value >= '0' && value <= '9';
}

[[nodiscard]] bool is_utc_rfc3339(const std::string_view value) noexcept {
    if (value.size() != 20 || value[4] != '-' || value[7] != '-' ||
        value[10] != 'T' || value[13] != ':' || value[16] != ':' ||
        value[19] != 'Z') {
        return false;
    }
    constexpr std::array<std::size_t, 14> digit_positions{
        0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
    for (const std::size_t position : digit_positions) {
        if (!is_ascii_digit(value[position])) {
            return false;
        }
    }
    return true;
}

void append_json_string(std::ostringstream& output,
                        const std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    output << '"';
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        switch (byte) {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                if (byte < 0x20U) {
                    output << "\\u00" << hex[byte >> 4U]
                           << hex[byte & 0x0FU];
                } else {
                    output << static_cast<char>(byte);
                }
        }
    }
    output << '"';
}

void append_string_field(std::ostringstream& output,
                         const std::string_view name,
                         const std::string_view value,
                         const bool trailing_comma = true) {
    append_json_string(output, name);
    output << ": ";
    append_json_string(output, value);
    if (trailing_comma) {
        output << ',';
    }
    output << '\n';
}

void invalid_metadata(const std::string& reason) {
    throw ResultError(ResultErrorCode::invalid_data,
                      "invalid experiment metadata: " + reason);
}

}  // namespace

BuildVersions collect_build_versions() {
    return {OPENSSL_VERSION_STR, OQS_VERSION_TEXT, compiler_version()};
}

std::string current_utc_rfc3339() {
    const std::time_t now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm utc{};
    if (gmtime_r(&now, &utc) == nullptr) {
        throw ResultError(ResultErrorCode::invalid_data,
                          "could not convert current time to UTC");
    }
    std::array<char, 21> buffer{};
    if (std::strftime(buffer.data(),
                      buffer.size(),
                      "%Y-%m-%dT%H:%M:%SZ",
                      &utc) != 20U) {
        throw ResultError(ResultErrorCode::invalid_data,
                          "could not format current UTC timestamp");
    }
    return buffer.data();
}

void validate_metadata(const ExperimentMetadata& metadata) {
    if (metadata.schema_version != metadata_schema_version) {
        invalid_metadata("unsupported schema version");
    }
    if (metadata.experiment_id == 0 ||
        !is_utc_rfc3339(metadata.created_at_utc)) {
        invalid_metadata(
            "experiment ID and RFC 3339 UTC timestamp are required");
    }
    const auto& configuration = metadata.configuration;
    if (configuration.warmup_iterations !=
            benchmark::warmup_iterations_per_run ||
        configuration.measured_iterations_per_run !=
            protocol::limits::iterations_per_run ||
        configuration.independent_repetitions != 3U ||
        configuration.payload_sizes_bytes !=
            std::vector<std::uint32_t>{protocol::limits::payload_10_kib,
                                       protocol::limits::payload_100_kib,
                                       protocol::limits::payload_1_mib} ||
        configuration.run_order != "randomized") {
        invalid_metadata("configuration differs from the fixed method");
    }
    try {
        static_cast<void>(
            benchmark::parse_payload_seed(configuration.payload_seed_hex));
    } catch (const std::invalid_argument&) {
        invalid_metadata("payload seed is not a 32-byte hexadecimal value");
    }

    std::set<std::uint32_t> run_ids;
    for (const auto& run : metadata.runs) {
        if (run.run_id == 0 || !run_ids.insert(run.run_id).second) {
            invalid_metadata("run IDs must be positive and unique");
        }
        if (run.repetition == 0 ||
            run.repetition > configuration.independent_repetitions ||
            protocol::algorithm_name(run.algorithm).empty() ||
            run.provider != expected_provider(run.algorithm) ||
            !is_supported_payload(run.payload_bytes) ||
            status_name(run.status).empty()) {
            invalid_metadata("run contains an invalid value");
        }
        if (run.status == RunStatus::completed &&
            (run.error_code != 0 || run.failed_iteration ||
             !run.error_detail.empty())) {
            invalid_metadata("completed run contains failure data");
        }
        if (run.status == RunStatus::aborted &&
            (run.error_code == 0 || run.error_code > 8U)) {
            invalid_metadata("aborted run requires a protocol error code");
        }
        if (run.failed_iteration &&
            *run.failed_iteration >=
                configuration.measured_iterations_per_run) {
            invalid_metadata("failed iteration is outside the run");
        }
    }
}

std::string serialize_metadata_json(const ExperimentMetadata& metadata) {
    validate_metadata(metadata);
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "{\n  \"schema_version\": " << metadata.schema_version
           << ",\n  \"experiment_id\": " << metadata.experiment_id
           << ",\n  ";
    append_string_field(output, "created_at_utc", metadata.created_at_utc);

    const auto& config = metadata.configuration;
    output << "  \"configuration\": {\n"
           << "    \"warmup_iterations\": " << config.warmup_iterations
           << ",\n    \"measured_iterations_per_run\": "
           << config.measured_iterations_per_run
           << ",\n    \"independent_repetitions\": "
           << config.independent_repetitions
           << ",\n    \"payload_sizes_bytes\": [";
    for (std::size_t index = 0; index < config.payload_sizes_bytes.size();
         ++index) {
        if (index != 0) {
            output << ", ";
        }
        output << config.payload_sizes_bytes[index];
    }
    output << "],\n    ";
    append_string_field(output, "payload_seed_hex", config.payload_seed_hex);
    output << "    ";
    append_string_field(output, "run_order", config.run_order, false);
    output << "  },\n  \"tool_versions\": {\n    ";
    append_string_field(output, "cmake_host", metadata.tool_versions.cmake_host);
    output << "    ";
    append_string_field(
        output, "cmake_android", metadata.tool_versions.cmake_android);
    output << "    ";
    append_string_field(output, "ninja", metadata.tool_versions.ninja);
    output << "    ";
    append_string_field(output, "python", metadata.tool_versions.python);
    output << "    ";
    append_string_field(
        output, "jdk_runtime", metadata.tool_versions.jdk_runtime);
    output << "    ";
    append_string_field(
        output, "java_target", metadata.tool_versions.java_target);
    output << "    ";
    append_string_field(output,
                        "android_gradle_plugin",
                        metadata.tool_versions.android_gradle_plugin);
    output << "    ";
    append_string_field(output, "gradle", metadata.tool_versions.gradle);
    output << "    ";
    append_string_field(output, "openssl", metadata.tool_versions.openssl);
    output << "    ";
    append_string_field(
        output, "liboqs", metadata.tool_versions.liboqs, false);
    output << "  },\n  \"client\": {\n    ";
    append_string_field(output, "platform", metadata.client.platform);
    output << "    ";
    append_string_field(output, "device_model", metadata.client.device_model);
    output << "    ";
    append_string_field(
        output, "android_version", metadata.client.android_version);
    output << "    ";
    append_string_field(output, "abi", metadata.client.abi);
    output << "    ";
    append_string_field(output, "ndk", metadata.client.ndk);
    output << "    ";
    append_string_field(output, "clang", metadata.client.clang, false);
    output << "  },\n  \"server\": {\n    ";
    append_string_field(output, "platform", metadata.server.platform);
    output << "    ";
    append_string_field(output, "cpu", metadata.server.cpu);
    output << "    ";
    append_string_field(output, "os", metadata.server.os);
    output << "    ";
    append_string_field(output, "kernel", metadata.server.kernel);
    output << "    ";
    append_string_field(output, "clang", metadata.server.clang, false);
    output << "  },\n  \"runs\": [";

    if (!metadata.runs.empty()) {
        output << '\n';
    }
    for (std::size_t index = 0; index < metadata.runs.size(); ++index) {
        const auto& run = metadata.runs[index];
        output << "    {\n      \"run_id\": " << run.run_id
               << ",\n      \"repetition\": " << run.repetition
               << ",\n      \"algorithm\": ";
        append_json_string(output, protocol::algorithm_name(run.algorithm));
        output << ",\n      \"provider\": ";
        append_json_string(output, run.provider);
        output << ",\n      \"payload_bytes\": " << run.payload_bytes
               << ",\n      \"status\": ";
        append_json_string(output, status_name(run.status));
        output << ",\n      \"error_code\": " << run.error_code
               << ",\n      \"failed_iteration\": ";
        if (run.failed_iteration) {
            output << *run.failed_iteration;
        } else {
            output << "null";
        }
        output << ",\n      \"error_detail\": ";
        append_json_string(output, run.error_detail);
        output << "\n    }";
        if (index + 1U != metadata.runs.size()) {
            output << ',';
        }
        output << '\n';
    }
    if (!metadata.runs.empty()) {
        output << "  ";
    }
    output << "]\n}\n";
    return output.str();
}

void write_metadata_json(const std::string& path,
                         const ExperimentMetadata& metadata) {
    const std::string serialized = serialize_metadata_json(metadata);
    std::ofstream output(path, std::ios::out | std::ios::trunc);
    output.imbue(std::locale::classic());
    if (!output) {
        throw ResultError(ResultErrorCode::open_failed,
                          "could not open metadata file: " + path);
    }
    output << serialized;
    output.flush();
    if (!output) {
        throw ResultError(ResultErrorCode::write_failed,
                          "could not write metadata file: " + path);
    }
}

}  // namespace tcc::results

#undef TCC_STRINGIFY
#undef TCC_STRINGIFY_IMPL
