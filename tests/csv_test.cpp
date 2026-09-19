#include "tcc/csv.hpp"
#include "tcc/payload.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <unistd.h>

namespace {

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

template <typename Callable>
void expect_invalid_argument(Callable&& callable) {
    try {
        callable();
        throw std::runtime_error("expected std::invalid_argument");
    } catch (const std::invalid_argument&) {
    }
}

template <typename Callable>
void expect_csv_error(Callable&& callable) {
    try {
        callable();
        throw std::runtime_error("expected tcc::CsvError");
    } catch (const tcc::CsvError&) {
    }
}

std::string create_temporary_file() {
    std::array<char, 64U> path{};
    constexpr std::string_view pattern = "/tmp/tcc-measurements-XXXXXX";
    std::copy(pattern.begin(), pattern.end(), path.begin());
    const int descriptor = ::mkstemp(path.data());
    if (descriptor < 0) {
        throw std::runtime_error("failed to create temporary CSV file");
    }
    ::close(descriptor);
    return path.data();
}

}  // namespace

int main() {
    std::string temporary_path;
    try {
        const tcc::Measurement successful{
            1U,
            "clientAndroid16",
            tcc::Side::client,
            tcc::Algorithm::ml_dsa_44,
            tcc::payload_10_kib,
            tcc::Operation::keygen,
            123456,
            true,
        };
        const tcc::Measurement failed{
            30U,
            "client, \"Android10\"",
            tcc::Side::server,
            tcc::Algorithm::ed25519,
            tcc::payload_1_mib,
            tcc::Operation::verify,
            789,
            false,
        };

        const std::string expected =
            "repetition,client,side,algorithm,payload_bytes,operation,"
            "duration_ns,status\n"
            "1,clientAndroid16,client,ML-DSA-44,10240,keygen,123456,success\n"
            "30,\"client, \"\"Android10\"\"\",server,Ed25519,1048576,verify,"
            "789,failure\n";

        std::ostringstream serialized;
        tcc::write_measurement_csv_header(serialized);
        tcc::write_measurement_csv_row(serialized, successful);
        tcc::write_measurement_csv_row(serialized, failed);
        expect(serialized.str() == expected, "unexpected CSV serialization");

        temporary_path = create_temporary_file();
        {
            tcc::MeasurementCsvWriter writer(temporary_path);
            writer.write(successful);
            writer.write(failed);
            writer.flush();
        }
        std::ifstream input(temporary_path);
        const std::string persisted{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
        expect(persisted == expected, "persisted CSV differs from serialization");

        tcc::Measurement invalid = successful;
        invalid.repetition = 0U;
        expect_invalid_argument([&] {
            tcc::write_measurement_csv_row(serialized, invalid);
        });
        invalid = successful;
        invalid.client.clear();
        expect_invalid_argument([&] {
            tcc::write_measurement_csv_row(serialized, invalid);
        });
        invalid = successful;
        invalid.side = static_cast<tcc::Side>(0xFF);
        expect_invalid_argument([&] {
            tcc::write_measurement_csv_row(serialized, invalid);
        });
        invalid = successful;
        invalid.algorithm = static_cast<tcc::Algorithm>(0xFF);
        expect_invalid_argument([&] {
            tcc::write_measurement_csv_row(serialized, invalid);
        });
        invalid = successful;
        invalid.payload_bytes = 1U;
        expect_invalid_argument([&] {
            tcc::write_measurement_csv_row(serialized, invalid);
        });
        invalid = successful;
        invalid.operation = static_cast<tcc::Operation>(0xFF);
        expect_invalid_argument([&] {
            tcc::write_measurement_csv_row(serialized, invalid);
        });
        invalid = successful;
        invalid.duration_ns = -1;
        expect_invalid_argument([&] {
            tcc::write_measurement_csv_row(serialized, invalid);
        });

        expect(
            tcc::side_name(tcc::Side::client) == "client" &&
                tcc::side_name(tcc::Side::server) == "server",
            "unexpected measurement side name");
        expect(
            tcc::operation_name(tcc::Operation::keygen) == "keygen" &&
                tcc::operation_name(tcc::Operation::sign) == "sign" &&
                tcc::operation_name(tcc::Operation::verify) == "verify",
            "unexpected measurement operation name");

        std::ostringstream failed_output;
        failed_output.setstate(std::ios::badbit);
        expect_csv_error([&] {
            tcc::write_measurement_csv_header(failed_output);
        });
    } catch (const std::exception& error) {
        if (!temporary_path.empty()) {
            std::remove(temporary_path.c_str());
        }
        std::cerr << error.what() << '\n';
        return 1;
    }

    std::remove(temporary_path.c_str());
    return 0;
}
