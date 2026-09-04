#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

#include "tcc/benchmark/measurement.hpp"

namespace tcc::results {

enum class ResultErrorCode {
    invalid_data,
    open_failed,
    write_failed,
};

class ResultError : public std::runtime_error {
  public:
    ResultError(ResultErrorCode code, std::string message);

    [[nodiscard]] ResultErrorCode code() const noexcept;

  private:
    ResultErrorCode code_;
};

class ResultWriter {
  public:
    virtual ~ResultWriter() = default;

    virtual void write(const benchmark::Measurement& measurement) = 0;
    virtual void flush() = 0;
    [[nodiscard]] virtual std::size_t rows_written() const noexcept = 0;
};

}  // namespace tcc::results
