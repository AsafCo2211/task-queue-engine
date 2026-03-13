cmake_minimum_required(VERSION 3.20)

# -----------------------------------------------------------------------
# Helper macro — creates a test executable and registers it with ctest
#
# Usage: add_gtest(test_broker test_broker.cpp)
# -----------------------------------------------------------------------
macro(add_gtest target source)
    add_executable(${target} ${source})

    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(${target} PRIVATE
        GTest::gtest_main
        Threads::Threads
        nlohmann_json::nlohmann_json
    )

    # Register with CTest so `ctest` or `cmake --build . --target test` runs it
    include(GoogleTest)
    gtest_discover_tests(${target})
endmacro()

# ── Test executables ─────────────────────────────────────────────────────
add_gtest(test_broker          test_broker.cpp)
add_gtest(test_scheduler       test_scheduler.cpp)
add_gtest(test_circuit_breaker test_circuit_breaker.cpp)
add_gtest(test_worker_pool     test_worker_pool.cpp)