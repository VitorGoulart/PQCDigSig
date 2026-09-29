#include "tcc/server/config.hpp"
#include "tcc/server/server.hpp"

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        const tcc::server::ServerConfig config =
            tcc::server::parse_server_config(argc, argv);
        if (config.show_help) {
            std::cout << tcc::server::server_usage();
            return 0;
        }

        const tcc::server::RunSummary summary = tcc::server::run(config);
        std::cout << "Experiment finished: " << summary.succeeded
                  << " successful, " << summary.failed << " failed\n";
        return summary.failed == 0U ? 0 : 1;
    } catch (const tcc::server::ConfigError& error) {
        std::cerr << "Configuration error: " << error.what() << "\n\n"
                  << tcc::server::server_usage();
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Server error: " << error.what() << '\n';
        return 1;
    }
}
