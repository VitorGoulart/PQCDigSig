#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "tcc/protocol/algorithm_id.hpp"

namespace tcc::results {

inline constexpr std::uint32_t metadata_schema_version = 1;

struct ExperimentConfiguration {
    std::uint32_t warmup_iterations = 20;
    std::uint32_t measured_iterations_per_run = 1000;
    std::uint32_t independent_repetitions = 3;
    std::vector<std::uint32_t> payload_sizes_bytes{10240, 102400, 1048576};
    std::string payload_seed_hex;
    std::string run_order = "randomized";
};

struct ToolVersions {
    std::string cmake_host;
    std::string cmake_android;
    std::string ninja;
    std::string python;
    std::string jdk_runtime;
    std::string java_target;
    std::string android_gradle_plugin;
    std::string gradle;
    std::string openssl;
    std::string liboqs;
};

struct ClientEnvironment {
    std::string platform = "Android";
    std::string device_model;
    std::string android_version;
    std::string abi;
    std::string ndk;
    std::string clang;
};

struct ServerEnvironment {
    std::string platform = "Linux";
    std::string cpu;
    std::string os;
    std::string kernel;
    std::string clang;
};

enum class RunStatus : std::uint8_t {
    completed,
    aborted,
};

struct RunMetadata {
    std::uint32_t run_id;
    std::uint32_t repetition;
    protocol::AlgorithmId algorithm;
    std::string provider;
    std::uint32_t payload_bytes;
    RunStatus status;
    std::uint16_t error_code = 0;
    std::optional<std::uint32_t> failed_iteration;
    std::string error_detail;
};

struct ExperimentMetadata {
    std::uint32_t schema_version = metadata_schema_version;
    std::uint32_t experiment_id;
    std::string created_at_utc;
    ExperimentConfiguration configuration;
    ToolVersions tool_versions;
    ClientEnvironment client;
    ServerEnvironment server;
    std::vector<RunMetadata> runs;
};

struct BuildVersions {
    std::string openssl;
    std::string liboqs;
    std::string compiler;
};

[[nodiscard]] BuildVersions collect_build_versions();
[[nodiscard]] std::string current_utc_rfc3339();
void validate_metadata(const ExperimentMetadata& metadata);
[[nodiscard]] std::string serialize_metadata_json(
    const ExperimentMetadata& metadata);
void write_metadata_json(const std::string& path,
                         const ExperimentMetadata& metadata);

}  // namespace tcc::results
