#pragma once

#include <cstddef>
#include <fstream>
#include <string>

#include "tcc/results/result_writer.hpp"

namespace tcc::results {

class CsvWriter final : public ResultWriter {
  public:
    explicit CsvWriter(const std::string& path);

    void write(const benchmark::Measurement& measurement) override;
    void flush() override;
    [[nodiscard]] std::size_t rows_written() const noexcept override;

  private:
    std::ofstream output_;
    std::size_t rows_written_ = 0;
};

}  // namespace tcc::results
