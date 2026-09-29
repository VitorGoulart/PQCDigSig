#pragma once

#include "tcc/server/config.hpp"

#include <cstdint>

namespace tcc::server {

struct RunSummary {
    std::uint64_t succeeded{};
    std::uint64_t failed{};
};

// Executa a matriz completa, aceitando uma conexão TCP por repetição.
[[nodiscard]] RunSummary run(const ServerConfig& config);

}  // namespace tcc::server
