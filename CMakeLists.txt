cmake_minimum_required(VERSION 3.22)

project(Dibyt
    VERSION 0.1.0
    DESCRIPTION "Self-hosted, parallel, resumable and fault-tolerant file distribution system"
    LANGUAGES C CXX
)

# Enforce Modern C++20 for all product targets
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clangd/IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Option to enable unit and integration tests
option(DIBYT_BUILD_TESTS "Build Dibyt unit, integration and smoke tests" ON)

# Compiler Warning Policy: Strict but guarded
function(dibyt_apply_warnings TARGET_NAME)
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
    else()
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wnon-virtual-dtor
            -Werror
        )
    endif()
endfunction()

# Target 1: Core Domain & Primitives Library
add_library(dibyt_core STATIC
    libs/core/src/core.cpp
)
add_library(dibyt::core ALIAS dibyt_core)

target_include_directories(dibyt_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/core/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(dibyt_core PUBLIC cxx_std_20)
dibyt_apply_warnings(dibyt_core)

# Target 2: Thin CLI Application Executable Skeleton
add_executable(dibyt-cli
    apps/dibyt-cli/src/main.cpp
)
target_link_libraries(dibyt-cli PRIVATE dibyt::core)
dibyt_apply_warnings(dibyt-cli)
set_target_properties(dibyt-cli PROPERTIES OUTPUT_NAME "dibyt")

# Target 3: Thin Server Application Executable Skeleton
add_executable(dibyt-server
    apps/dibyt-server/src/main.cpp
)
target_link_libraries(dibyt-server PRIVATE dibyt::core)
dibyt_apply_warnings(dibyt-server)

# Target 4: Testing & Smoke Verification
if(DIBYT_BUILD_TESTS)
    enable_testing()

    add_executable(test_core_smoke
        tests/unit/core/test_core_smoke.cpp
    )
    target_link_libraries(test_core_smoke PRIVATE dibyt::core)
    dibyt_apply_warnings(test_core_smoke)

    add_test(NAME CoreSmokeTest COMMAND test_core_smoke)
endif()
