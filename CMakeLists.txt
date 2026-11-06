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

# Target 2: Collection Scanner and Manifest Subsystem
add_library(dibyt_manifest STATIC
    libs/manifest/src/manifest.cpp
)
add_library(dibyt::manifest ALIAS dibyt_manifest)

target_include_directories(dibyt_manifest
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/manifest/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(dibyt_manifest PUBLIC dibyt::core)
target_compile_features(dibyt_manifest PUBLIC cxx_std_20)
dibyt_apply_warnings(dibyt_manifest)

# Target 3: Protocol Wire Codec Subsystem
add_library(dibyt_protocol STATIC
    libs/protocol/src/codec.cpp
)
add_library(dibyt::protocol ALIAS dibyt_protocol)

target_include_directories(dibyt_protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/protocol/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(dibyt_protocol PUBLIC dibyt::core)
target_compile_features(dibyt_protocol PUBLIC cxx_std_20)
dibyt_apply_warnings(dibyt_protocol)

# Target 4: Networking and TCP Transport Subsystem
add_library(dibyt_networking STATIC
    libs/networking/src/tcp.cpp
)
add_library(dibyt::networking ALIAS dibyt_networking)

target_include_directories(dibyt_networking
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/networking/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(dibyt_networking PUBLIC dibyt::core dibyt::protocol)
if(WIN32)
    target_link_libraries(dibyt_networking PUBLIC ws2_32)
endif()
target_compile_features(dibyt_networking PUBLIC cxx_std_20)
dibyt_apply_warnings(dibyt_networking)

# Target 5: Transfer Staging and Atomic Finalization Subsystem
add_library(dibyt_transfer STATIC
    libs/transfer/src/staging.cpp
)
add_library(dibyt::transfer ALIAS dibyt_transfer)

target_include_directories(dibyt_transfer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/transfer/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(dibyt_transfer PUBLIC dibyt::core dibyt::manifest)
target_compile_features(dibyt_transfer PUBLIC cxx_std_20)
dibyt_apply_warnings(dibyt_transfer)

# Target 6: Thin CLI Application Executable Skeleton
add_executable(dibyt-cli
    apps/dibyt-cli/src/main.cpp
)
target_link_libraries(dibyt-cli PRIVATE dibyt::core dibyt::manifest dibyt::protocol dibyt::networking dibyt::transfer)
dibyt_apply_warnings(dibyt-cli)
set_target_properties(dibyt-cli PROPERTIES OUTPUT_NAME "dibyt")

# Target 7: Thin Server Application Executable Skeleton
add_executable(dibyt-server
    apps/dibyt-server/src/main.cpp
)
target_link_libraries(dibyt-server PRIVATE dibyt::core dibyt::manifest dibyt::protocol dibyt::networking dibyt::transfer)
dibyt_apply_warnings(dibyt-server)

# Target 8: Testing & Smoke Verification
if(DIBYT_BUILD_TESTS)
    enable_testing()

    add_executable(test_core_smoke
        tests/unit/core/test_core_smoke.cpp
    )
    target_link_libraries(test_core_smoke PRIVATE dibyt::core)
    dibyt_apply_warnings(test_core_smoke)
    add_test(NAME CoreSmokeTest COMMAND test_core_smoke)

    add_executable(test_manifest
        tests/unit/manifest/test_manifest.cpp
    )
    target_link_libraries(test_manifest PRIVATE dibyt::core dibyt::manifest)
    dibyt_apply_warnings(test_manifest)
    add_test(NAME ManifestUnitTest COMMAND test_manifest)

    add_executable(test_protocol
        tests/unit/protocol/test_protocol.cpp
    )
    target_link_libraries(test_protocol PRIVATE dibyt::core dibyt::protocol)
    dibyt_apply_warnings(test_protocol)
    add_test(NAME ProtocolUnitTest COMMAND test_protocol)

    add_executable(test_tcp
        tests/unit/networking/test_tcp.cpp
    )
    target_link_libraries(test_tcp PRIVATE dibyt::core dibyt::protocol dibyt::networking)
    dibyt_apply_warnings(test_tcp)
    add_test(NAME TcpTransportUnitTest COMMAND test_tcp)

    add_executable(test_staging
        tests/unit/transfer/test_staging.cpp
    )
    target_link_libraries(test_staging PRIVATE dibyt::core dibyt::manifest dibyt::transfer)
    dibyt_apply_warnings(test_staging)
    add_test(NAME StagingUnitTest COMMAND test_staging)
endif()
