#include <iostream>
#include "dibyt/core/version.hpp"
#include "dibyt/core/error.hpp"

// Thin CLI entry point: delegates all business logic to libs/
int main(int argc, char* argv[]) {
    std::cout << dibyt::core::kProductName << " CLI v" 
              << dibyt::core::kProductVersion << " — " 
              << dibyt::core::kProductTagline << "\n";
    std::cout << "Status: Engineering Preview (Core Foundation)\n";
    return 0;
}
