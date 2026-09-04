#include "tcc/results/csv_writer.hpp"

#include <cstdint>
#include <ios>
#include <locale>
#include <string>

#include "tcc/protocol/protocol_limits.hpp"

namespace tcc::results {
namespace {

void validate_measurement(const benchmark::Measurement& measurement) {
    if (measurement.experiment_id == 0 || measurement.run_id == 0 ||
        measurement.iteration >= protocol::limits::iterations_per_run ||
        protocol::algorithm_name(measurement.algorithm).empty() ||
        measurement.duration_ns < 0) {
        throw ResultError(ResultErrorCode::invalid_data,
                          "measurement contains an invalid value");
    }
    if (measurement.payload_bytes != protocol::limits::payload_10_kib &&
        measurement.payload_bytes != protocol::limits::payload_100_kib &&
        measurement.payload_bytes != protocol::limits::payload_1_mib) {
        throw ResultError(ResultErrorCode::invalid_data,
                          "measurement contains an invalid payload size");
    }
    if (benchmark::to_string(measurement.side) == "unknown" ||
        benchmark::to_string(measurement.operation) == "unknown") {
        throw ResultError(ResultErrorCode::invalid_data,
                          "measurement contains an invalid enum value");
    }
}

}  // namespace

CsvWriter::CsvWriter(const std::string& path)
    : output_(path, std::ios::out | std::ios::trunc | std::ios::binary) {
    output_.imbue(std::locale::classic());
    if (!output_) {
        throw ResultError(ResultErrorCode::open_failed,
                          "could not open CSV result file: " + path);
    }
    output_ << "experiment_id,run_id,iteration,side,algorithm,payload_bytes,"
               "operation,duration\n";
    if (!output_) {
        throw ResultError(ResultErrorCode::write_failed,
                          "could not write CSV header");
    }
}

void CsvWriter::write(const benchmark::Measurement& measurement) {
    validate_measurement(measurement);
    output_ << measurement.experiment_id << ',' << measurement.run_id << ','
            << measurement.iteration << ','
            << benchmark::to_string(measurement.side) << ','
            << protocol::algorithm_name(measurement.algorithm) << ','
            << measurement.payload_bytes << ','
            << benchmark::to_string(measurement.operation) << ','
            << measurement.duration_ns << '\n';
    if (!output_) {
        throw ResultError(ResultErrorCode::write_failed,
                          "could not write CSV measurement");
    }
    ++rows_written_;
}

void CsvWriter::flush() {
    output_.flush();
    if (!output_) {
        throw ResultError(ResultErrorCode::write_failed,
                          "could not flush CSV result file");
    }
}

std::size_t CsvWriter::rows_written() const noexcept {
    return rows_written_;
}

}  // namespace tcc::results
