cmake_minimum_required(VERSION 3.20)
project(TaskQueueEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ── Main executable ──────────────────────────────────────────────────────
add_executable(TaskQueue src/main.cpp)
target_include_directories(TaskQueue PRIVATE include)

# ── Threading ────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)
target_link_libraries(TaskQueue PRIVATE Threads::Threads)

# ── nlohmann/json ────────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
set(JSON_Install    OFF CACHE INTERNAL "")
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

target_link_libraries(TaskQueue PRIVATE nlohmann_json::nlohmann_json)

# ── GoogleTest ───────────────────────────────────────────────────────────
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── Tests ────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)