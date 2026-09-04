#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

#include "tcc/benchmark/measurement.hpp"
#include "tcc/benchmark/payload_generator.hpp"
#include "tcc/protocol/protocol_limits.hpp"
#include "tcc/results/csv_writer.hpp"
#include "tcc/results/metadata.hpp"
#include "tcc/results/result_writer.hpp"
#include "../protocol/test_support.hpp"

namespace {

using tcc::benchmark::Measurement;
using tcc::benchmark::Operation;
using tcc::benchmark::ParticipantSide;
using tcc::protocol::AlgorithmId;
using tcc::results::ResultError;
using tcc::results::ResultErrorCode;

constexpr char csv_path[] = "/tmp/tcc_result_writer_test.csv";
constexpr char json_path[] = "/tmp/tcc_result_writer_test.json";

template <typename Function>
void expect_result_error(Function&& function,
                         const ResultErrorCode expected_code,
                         const std::string& message) {
    try {
        function();
    } catch (const ResultError& error) {
        expect(error.code() == expected_code,
               "unexpected result error code");
        return;
    }
    throw std::runtime_error(message);
}

[[nodiscard]] std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not read generated test file");
    }
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

[[nodiscard]] Measurement measurement(const Operation operation,
                                      const std::int64_t duration) {
    return {17,
            4,
            0,
            ParticipantSide::client,
            AlgorithmId::ed25519,
            tcc::protocol::limits::payload_10_kib,
            operation,
            duration};
}

[[nodiscard]] tcc::results::ExperimentMetadata make_metadata() {
    tcc::results::ExperimentMetadata metadata{};
    metadata.experiment_id = 17;
    metadata.created_at_utc = "2026-09-04T18:30:00Z";
    metadata.configuration.payload_seed_hex =
        "000102030405060708090a0b0c0d0e0f"
        "101112131415161718191a1b1c1d1e1f";
    const auto versions = tcc::results::collect_build_versions();
    metadata.tool_versions.openssl = versions.openssl;
    metadata.tool_versions.liboqs = versions.liboqs;
    metadata.server.clang = versions.compiler;
    metadata.client.abi = "arm64-v8a";
    metadata.runs.push_back({4,
                             1,
                             AlgorithmId::ed25519,
                             "OpenSSL",
                             tcc::protocol::limits::payload_10_kib,
                             tcc::results::RunStatus::completed,
                             0,
                             std::nullopt,
                             {}});
    metadata.runs.push_back({5,
                             1,
                             AlgorithmId::falcon_512,
                             "liboqs",
                             tcc::protocol::limits::payload_100_kib,
                             tcc::results::RunStatus::aborted,
                             5,
                             12,
                             "assinatura \"inválida\"\nrecebida"});
    return metadata;
}

void test_csv_writer() {
    static_cast<void>(std::remove(csv_path));
    {
        tcc::results::CsvWriter writer(csv_path);
        tcc::results::ResultWriter& polymorphic_writer = writer;
        polymorphic_writer.write(measurement(Operation::key_generation, 123));
        polymorphic_writer.write(measurement(Operation::signing, 456));
        polymorphic_writer.write(measurement(Operation::verification, 789));
        polymorphic_writer.flush();
        expect(polymorphic_writer.rows_written() == 3,
               "CSV writer returned the wrong row count");
    }

    const std::string expected =
        "experiment_id,run_id,iteration,side,algorithm,payload_bytes,"
        "operation,duration\n"
        "17,4,0,client,Ed25519,10240,keygen,123\n"
        "17,4,0,client,Ed25519,10240,sign,456\n"
        "17,4,0,client,Ed25519,10240,verify,789\n";
    expect(read_file(csv_path) == expected,
           "CSV output does not match the normative schema");

    auto invalid = measurement(Operation::signing, -1);
    tcc::results::CsvWriter writer(csv_path);
    expect_result_error(
        [&] { writer.write(invalid); },
        ResultErrorCode::invalid_data,
        "negative duration was written to CSV");
    invalid = measurement(Operation::signing, 1);
    invalid.iteration = tcc::protocol::limits::iterations_per_run;
    expect_result_error(
        [&] { writer.write(invalid); },
        ResultErrorCode::invalid_data,
        "out-of-range iteration was written to CSV");
    invalid = measurement(Operation::signing, 1);
    invalid.algorithm = AlgorithmId::invalid;
    expect_result_error(
        [&] { writer.write(invalid); },
        ResultErrorCode::invalid_data,
        "invalid algorithm was written to CSV");
    invalid = measurement(Operation::signing, 1);
    invalid.side = static_cast<ParticipantSide>(0xFFU);
    expect_result_error(
        [&] { writer.write(invalid); },
        ResultErrorCode::invalid_data,
        "invalid participant side was written to CSV");
    invalid = measurement(Operation::signing, 1);
    invalid.operation = static_cast<Operation>(0xFFU);
    expect_result_error(
        [&] { writer.write(invalid); },
        ResultErrorCode::invalid_data,
        "invalid operation was written to CSV");
    expect_result_error(
        [] {
            tcc::results::CsvWriter missing_directory(
                "/definitely-missing-tcc-directory/results.csv");
        },
        ResultErrorCode::open_failed,
        "CSV writer accepted a missing output directory");
    expect_result_error(
        [] {
            tcc::results::CsvWriter full_device("/dev/full");
            full_device.write(measurement(Operation::signing, 1));
            full_device.flush();
        },
        ResultErrorCode::write_failed,
        "CSV writer did not report a device write failure");
    static_cast<void>(std::remove(csv_path));
}

void test_metadata_serialization_and_file() {
    static_cast<void>(std::remove(json_path));
    const auto metadata = make_metadata();
    const std::string serialized =
        tcc::results::serialize_metadata_json(metadata);
    expect(serialized.find("\"schema_version\": 1") != std::string::npos &&
               serialized.find("\"algorithm\": \"Ed25519\"") !=
                   std::string::npos &&
               serialized.find("\"failed_iteration\": null") !=
                   std::string::npos &&
               serialized.find("\"failed_iteration\": 12") !=
                   std::string::npos &&
               serialized.find("assinatura \\\"inválida\\\"\\nrecebida") !=
                   std::string::npos,
           "metadata JSON omitted or incorrectly escaped a normative field");
    expect(serialized.find("duration") == std::string::npos,
           "metadata JSON contains a forbidden duration field");

    tcc::results::write_metadata_json(json_path, metadata);
    expect(read_file(json_path) == serialized,
           "metadata file differs from the serialized JSON");
    static_cast<void>(std::remove(json_path));

    const auto versions = tcc::results::collect_build_versions();
    expect(versions.openssl == "3.5.7" && versions.liboqs == "0.16.0" &&
               !versions.compiler.empty(),
           "compiled dependency versions were not collected");
    const std::string now = tcc::results::current_utc_rfc3339();
    expect(now.size() == 20 && now[4] == '-' && now[10] == 'T' &&
               now.back() == 'Z',
           "current UTC timestamp is not RFC 3339");

    expect_result_error(
        [&] {
            tcc::results::write_metadata_json(
                "/definitely-missing-tcc-directory/metadata.json", metadata);
        },
        ResultErrorCode::open_failed,
        "metadata writer accepted a missing output directory");
    expect_result_error(
        [&] { tcc::results::write_metadata_json("/dev/full", metadata); },
        ResultErrorCode::write_failed,
        "metadata writer did not report a device write failure");
}

void test_metadata_validation() {
    auto metadata = make_metadata();
    metadata.configuration.payload_seed_hex = "00";
    expect_result_error(
        [&] { static_cast<void>(
                  tcc::results::serialize_metadata_json(metadata)); },
        ResultErrorCode::invalid_data,
        "invalid payload seed was serialized");

    metadata = make_metadata();
    metadata.runs[0].error_code = 5;
    expect_result_error(
        [&] { tcc::results::validate_metadata(metadata); },
        ResultErrorCode::invalid_data,
        "completed run with failure data was accepted");

    metadata = make_metadata();
    metadata.runs[1].error_code = 0;
    expect_result_error(
        [&] { tcc::results::validate_metadata(metadata); },
        ResultErrorCode::invalid_data,
        "aborted run without error code was accepted");

    metadata = make_metadata();
    metadata.runs[1].run_id = metadata.runs[0].run_id;
    expect_result_error(
        [&] { tcc::results::validate_metadata(metadata); },
        ResultErrorCode::invalid_data,
        "duplicate run ID was accepted");

    metadata = make_metadata();
    metadata.runs[0].provider = "liboqs";
    expect_result_error(
        [&] { tcc::results::validate_metadata(metadata); },
        ResultErrorCode::invalid_data,
        "provider inconsistent with the algorithm was accepted");

    metadata = make_metadata();
    metadata.created_at_utc = "2026-09-04 18:30:00";
    expect_result_error(
        [&] { tcc::results::validate_metadata(metadata); },
        ResultErrorCode::invalid_data,
        "non-RFC-3339 timestamp was accepted");
}

}  // namespace

int main() {
    try {
        test_csv_writer();
        test_metadata_serialization_and_file();
        test_metadata_validation();
    } catch (const std::exception& error) {
        static_cast<void>(std::remove(csv_path));
        static_cast<void>(std::remove(json_path));
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
