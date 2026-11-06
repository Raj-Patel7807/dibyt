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

# Configure test targets with strict warnings and active assertions.
function(dibyt_configure_test TARGET_NAME)
    dibyt_apply_warnings(${TARGET_NAME})

    # Release builds define NDEBUG, which disables assert().
    # Undefine it for test targets so assertions remain active.
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${TARGET_NAME} PRIVATE -UNDEBUG)
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

# Target 5: Transfer Staging, Checkpoints and Parallel Engine Subsystem
add_library(dibyt_transfer STATIC
    libs/transfer/src/staging.cpp
    libs/transfer/src/checkpoint.cpp
    libs/transfer/src/engine.cpp
)
add_library(dibyt::transfer ALIAS dibyt_transfer)

target_include_directories(dibyt_transfer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/transfer/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(dibyt_transfer PUBLIC
    dibyt::core
    dibyt::manifest
    dibyt::storage
    dibyt::scheduler
    dibyt::networking
    dibyt::protocol
)
target_compile_features(dibyt_transfer PUBLIC cxx_std_20)
dibyt_apply_warnings(dibyt_transfer)

# Target 6: Persistent Storage and Metadata Subsystem
add_library(dibyt_storage STATIC
    libs/storage/src/database.cpp
)
add_library(dibyt::storage ALIAS dibyt_storage)

target_include_directories(dibyt_storage
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/storage/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(dibyt_storage PUBLIC dibyt::core)
target_compile_features(dibyt_storage PUBLIC cxx_std_20)
dibyt_apply_warnings(dibyt_storage)

# Target 7: Dynamic Work Scheduler and Worker Pool Subsystem
add_library(dibyt_scheduler STATIC
    libs/scheduler/src/scheduler.cpp
)
add_library(dibyt::scheduler ALIAS dibyt_scheduler)

target_include_directories(dibyt_scheduler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/scheduler/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(dibyt_scheduler PUBLIC dibyt::core dibyt::storage)
target_compile_features(dibyt_scheduler PUBLIC cxx_std_20)
dibyt_apply_warnings(dibyt_scheduler)

# Target 8: Configuration, Logging, and Observability Subsystem
add_library(dibyt_observability STATIC
    libs/observability/src/config.cpp
    libs/observability/src/logger.cpp
)
add_library(dibyt::observability ALIAS dibyt_observability)

target_include_directories(dibyt_observability
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/observability/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(dibyt_observability PUBLIC dibyt::core)
target_compile_features(dibyt_observability PUBLIC cxx_std_20)
dibyt_apply_warnings(dibyt_observability)

# Target 9: Security Foundations, Authentication, and Filesystem Jail Subsystem
add_library(dibyt_security STATIC
    libs/security/src/auth.cpp
)
add_library(dibyt::security ALIAS dibyt_security)

target_include_directories(dibyt_security
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/security/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(dibyt_security PUBLIC dibyt::core dibyt::manifest)
target_compile_features(dibyt_security PUBLIC cxx_std_20)
dibyt_apply_warnings(dibyt_security)

# Target 10: Thin CLI Application Executable
add_executable(dibyt-cli
    apps/dibyt-cli/src/main.cpp
    apps/dibyt-cli/src/cli_parser.cpp
)
target_include_directories(dibyt-cli PRIVATE apps/dibyt-cli/src)
target_link_libraries(dibyt-cli PRIVATE
    dibyt::core
    dibyt::manifest
    dibyt::protocol
    dibyt::networking
    dibyt::transfer
    dibyt::storage
    dibyt::scheduler
    dibyt::observability
    dibyt::security
)
dibyt_apply_warnings(dibyt-cli)
set_target_properties(dibyt-cli PROPERTIES OUTPUT_NAME "dibyt")

# Target 11: Thin Server Application Executable Skeleton
add_executable(dibyt-server
    apps/dibyt-server/src/main.cpp
)
target_link_libraries(dibyt-server PRIVATE
    dibyt::core
    dibyt::manifest
    dibyt::protocol
    dibyt::networking
    dibyt::transfer
    dibyt::storage
    dibyt::scheduler
    dibyt::observability
    dibyt::security
)
dibyt_apply_warnings(dibyt-server)

# Target 12: Embedded Management REST API Application
add_executable(dibyt-api
    apps/dibyt-api/src/router.cpp
)
target_include_directories(dibyt-api PRIVATE apps/dibyt-api/include)
target_link_libraries(dibyt-api PRIVATE
    dibyt::core
    dibyt::storage
    dibyt::security
    dibyt::observability
)
dibyt_apply_warnings(dibyt-api)

# Testing & Verification
if(DIBYT_BUILD_TESTS)
    enable_testing()

    add_executable(test_core_smoke
        tests/unit/core/test_core_smoke.cpp
    )
    target_link_libraries(test_core_smoke PRIVATE dibyt::core)
    dibyt_configure_test(test_core_smoke)
    add_test(NAME CoreSmokeTest COMMAND test_core_smoke)

    add_executable(test_cli_parser
        tests/unit/core/test_cli_parser.cpp
        apps/dibyt-cli/src/cli_parser.cpp
    )
    target_include_directories(test_cli_parser PRIVATE apps/dibyt-cli/src)
    target_link_libraries(test_cli_parser PRIVATE dibyt::core)
    dibyt_configure_test(test_cli_parser)
    add_test(NAME CliParserUnitTest COMMAND test_cli_parser)

    add_executable(test_manifest
        tests/unit/manifest/test_manifest.cpp
    )
    target_link_libraries(test_manifest PRIVATE dibyt::core dibyt::manifest)
    dibyt_configure_test(test_manifest)
    add_test(NAME ManifestUnitTest COMMAND test_manifest)

    add_executable(test_protocol
        tests/unit/protocol/test_protocol.cpp
    )
    target_link_libraries(test_protocol PRIVATE dibyt::core dibyt::protocol)
    dibyt_configure_test(test_protocol)
    add_test(NAME ProtocolUnitTest COMMAND test_protocol)

    add_executable(test_tcp
        tests/unit/networking/test_tcp.cpp
    )
    target_link_libraries(test_tcp PRIVATE dibyt::core dibyt::protocol dibyt::networking)
    dibyt_configure_test(test_tcp)
    add_test(NAME TcpTransportUnitTest COMMAND test_tcp)

    add_executable(test_staging
        tests/unit/transfer/test_staging.cpp
    )
    target_link_libraries(test_staging PRIVATE dibyt::core dibyt::manifest dibyt::transfer)
    dibyt_configure_test(test_staging)
    add_test(NAME StagingUnitTest COMMAND test_staging)

    add_executable(test_database
        tests/unit/storage/test_database.cpp
    )
    target_link_libraries(test_database PRIVATE dibyt::core dibyt::storage)
    dibyt_configure_test(test_database)
    add_test(NAME StorageDatabaseUnitTest COMMAND test_database)

    add_executable(test_checkpoint
        tests/unit/transfer/test_checkpoint.cpp
    )
    target_link_libraries(test_checkpoint PRIVATE dibyt::core dibyt::storage dibyt::transfer)
    dibyt_configure_test(test_checkpoint)
    add_test(NAME CheckpointUnitTest COMMAND test_checkpoint)

    add_executable(test_scheduler
        tests/unit/scheduler/test_scheduler.cpp
    )
    target_link_libraries(test_scheduler PRIVATE dibyt::core dibyt::storage dibyt::scheduler)
    dibyt_configure_test(test_scheduler)
    add_test(NAME SchedulerUnitTest COMMAND test_scheduler)

    add_executable(test_engine
        tests/unit/transfer/test_engine.cpp
    )
    target_link_libraries(test_engine PRIVATE
        dibyt::core
        dibyt::manifest
        dibyt::storage
        dibyt::scheduler
        dibyt::transfer
        dibyt::networking
        dibyt::protocol
    )
    dibyt_configure_test(test_engine)
    add_test(NAME EngineParallelTransferTest COMMAND test_engine)

    add_executable(test_recovery
        tests/unit/transfer/test_recovery.cpp
    )
    target_link_libraries(test_recovery PRIVATE dibyt::core dibyt::transfer)
    dibyt_configure_test(test_recovery)
    add_test(NAME FaultRecoveryUnitTest COMMAND test_recovery)

    add_executable(test_observability
        tests/unit/observability/test_observability.cpp
    )
    target_link_libraries(test_observability PRIVATE dibyt::core dibyt::observability)
    dibyt_configure_test(test_observability)
    add_test(NAME ObservabilityUnitTest COMMAND test_observability)

    add_executable(test_security
        tests/unit/security/test_security.cpp
    )
    target_link_libraries(test_security PRIVATE dibyt::core dibyt::manifest dibyt::security)
    dibyt_configure_test(test_security)
    add_test(NAME SecurityUnitTest COMMAND test_security)

    add_executable(test_api
        tests/unit/api/test_api.cpp
        apps/dibyt-api/src/router.cpp
    )
    target_include_directories(test_api PRIVATE apps/dibyt-api/include)
    target_link_libraries(test_api PRIVATE
        dibyt::core
        dibyt::manifest
        dibyt::storage
        dibyt::security
        dibyt::observability
    )
    dibyt_configure_test(test_api)
    add_test(NAME RestApiUnitTest COMMAND test_api)

    add_executable(test_fault_injection
        tests/fault_injection/test_fault_injection.cpp
    )
    target_link_libraries(test_fault_injection PRIVATE dibyt::core dibyt::storage dibyt::transfer)
    dibyt_configure_test(test_fault_injection)
    add_test(NAME FaultInjectionUnitTest COMMAND test_fault_injection)

    add_executable(benchmark_runner
        benchmarks/harness/benchmark_runner.cpp
    )
    target_link_libraries(benchmark_runner PRIVATE
        dibyt::core
        dibyt::manifest
        dibyt::storage
        dibyt::scheduler
        dibyt::transfer
        dibyt::networking
        dibyt::protocol
    )
    dibyt_apply_warnings(benchmark_runner)
endif()
