#include "tcc/results/result_writer.hpp"

#include <utility>

namespace tcc::results {

ResultError::ResultError(const ResultErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

ResultErrorCode ResultError::code() const noexcept {
    return code_;
}

}  // namespace tcc::results
