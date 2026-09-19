#include "tcc/csv.hpp"

#include "tcc/payload.hpp"

#include <string_view>

namespace tcc {
namespace {

constexpr std::string_view csv_header =
    "repetition,client,side,algorithm,payload_bytes,operation,duration_ns,status\n";

void ensure_stream(const std::ostream& output) {
    if (!output) {
        throw CsvError("failed to write measurement CSV");
    }
}

void write_csv_field(std::ostream& output, const std::string_view field) {
    const bool requires_quotes =
        field.find_first_of(",\"\r\n") != std::string_view::npos;
    if (!requires_quotes) {
        output << field;
        return;
    }

    output.put('"');
    for (const char character : field) {
        if (character == '"') {
            output.put('"');
        }
        output.put(character);
    }
    output.put('"');
}

void validate_measurement(const Measurement& measurement) {
    if (measurement.repetition == 0U) {
        throw std::invalid_argument("measurement repetition must start at one");
    }
    if (measurement.client.empty()) {
        throw std::invalid_argument("measurement client must not be empty");
    }
    static_cast<void>(side_name(measurement.side));
    if (!is_supported_algorithm(measurement.algorithm)) {
        throw std::invalid_argument("measurement algorithm is not supported");
    }
    if (!is_supported_payload_size(measurement.payload_bytes)) {
        throw std::invalid_argument("measurement payload size is not supported");
    }
    static_cast<void>(operation_name(measurement.operation));
    if (measurement.duration_ns < 0) {
        throw std::invalid_argument("measurement duration must not be negative");
    }
}

}  // namespace

void write_measurement_csv_header(std::ostream& output) {
    output << csv_header;
    ensure_stream(output);
}

void write_measurement_csv_row(
    std::ostream& output,
    const Measurement& measurement) {
    validate_measurement(measurement);

    output << measurement.repetition << ',';
    write_csv_field(output, measurement.client);
    output << ',' << side_name(measurement.side) << ','
           << algorithm_name(measurement.algorithm) << ','
           << measurement.payload_bytes << ','
           << operation_name(measurement.operation) << ','
           << measurement.duration_ns << ','
           << (measurement.success ? "success" : "failure") << '\n';
    ensure_stream(output);
}

MeasurementCsvWriter::MeasurementCsvWriter(const std::string& path)
    : output_(path, std::ios::out | std::ios::trunc) {
    if (!output_) {
        throw CsvError("failed to open measurement CSV: " + path);
    }
    write_measurement_csv_header(output_);
}

void MeasurementCsvWriter::write(const Measurement& measurement) {
    write_measurement_csv_row(output_, measurement);
}

void MeasurementCsvWriter::flush() {
    output_.flush();
    ensure_stream(output_);
}

}  // namespace tcc
