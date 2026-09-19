#include "tcc/payload.hpp"

#include <stdexcept>

namespace tcc {

bool is_supported_payload_size(const std::size_t size) noexcept {
    return size == payload_10_kib || size == payload_100_kib ||
           size == payload_1_mib;
}

Bytes make_payload(const std::size_t size) {
    if (!is_supported_payload_size(size)) {
        throw std::invalid_argument("unsupported payload size");
    }
    return Bytes(size, 0x65U);
}

}  // namespace tcc
