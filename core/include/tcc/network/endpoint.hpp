#pragma once

#include <cstdint>
#include <string>

namespace tcc::network {

struct Endpoint {
    std::string host;
    std::uint16_t port;
};

}  // namespace tcc::network
