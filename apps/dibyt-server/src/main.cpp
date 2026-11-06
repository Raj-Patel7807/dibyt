#include <iostream>
#include "dibyt/core/version.hpp"
#include "dibyt/core/error.hpp"

// Thin daemon entry point: delegates all server orchestration to libs/
int main(int argc, char* argv[]) {
    std::cout << dibyt::core::kProductName << " Server Daemon v" 
              << dibyt::core::kProductVersion << "\n";
    std::cout << "Status: Engineering Preview (Core Foundation)\n";
    return 0;
}
