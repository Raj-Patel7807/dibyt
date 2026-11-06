#include <iostream>
#include "dibyt/core/version.hpp"
#include "dibyt/core/error.hpp"

// Thin daemon entry point: command-line arguments reserved for future configuration
// integration with libs/observability config and libs/security auth setup.
int main(int /*argc*/, char* /*argv*/[]) {
    std::cout << dibyt::core::kProductName << " Server Daemon v"
              << dibyt::core::kProductVersion << "\n";
    std::cout << "Status: Engineering Preview (Core Foundation)\n";
    return 0;
}
