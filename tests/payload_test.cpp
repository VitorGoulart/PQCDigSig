#include "tcc/payload.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

int main() {
    const auto validate_payload = [](const std::size_t size) {
        const tcc::Bytes payload = tcc::make_payload(size);
        assert(payload.size() == size);
        assert(std::all_of(
            payload.cbegin(), payload.cend(),
            [](const std::uint8_t byte) { return byte == 0x65U; }));
    };

    validate_payload(tcc::payload_10_kib);
    validate_payload(tcc::payload_100_kib);
    validate_payload(tcc::payload_1_mib);

    try {
        static_cast<void>(tcc::make_payload(1U));
        return 1;
    } catch (const std::invalid_argument&) {
        return 0;
    }
}
