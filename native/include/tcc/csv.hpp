#pragma once

#include "tcc/measurement.hpp"

#include <fstream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace tcc {

class CsvError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void write_measurement_csv_header(std::ostream& output);
void write_measurement_csv_row(
    std::ostream& output,
    const Measurement& measurement);

class MeasurementCsvWriter final {
public:
    explicit MeasurementCsvWriter(const std::string& path);

    MeasurementCsvWriter(const MeasurementCsvWriter&) = delete;
    MeasurementCsvWriter& operator=(const MeasurementCsvWriter&) = delete;
    MeasurementCsvWriter(MeasurementCsvWriter&&) = delete;
    MeasurementCsvWriter& operator=(MeasurementCsvWriter&&) = delete;

    void write(const Measurement& measurement);
    void flush();

private:
    std::ofstream output_;
};

}  // namespace tcc
