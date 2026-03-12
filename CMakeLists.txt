cmake_minimum_required(VERSION 3.20)
project(TaskQueueEngine VERSION 0.1.0 LANGUAGES CXX)

# -----------------------------------------------------------------------
# C++20 is required throughout the project.
# We enforce it here so every target inherits the standard automatically.
# -----------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Use -std=c++20, not -std=gnu++20

# -----------------------------------------------------------------------
# Compiler warnings — treat the project as production code from day one.
# -Wall / -Wextra catch the majority of real bugs.
# -Wpedantic enforces strict ISO C++ compliance.
# -----------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------
# Build type defaults to Debug when not specified.
# Use: cmake -DCMAKE_BUILD_TYPE=Release .. for a release build.
# -----------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# -----------------------------------------------------------------------
# Main executable — we'll add more source files here as we build them.
# -----------------------------------------------------------------------
add_executable(TaskQueue src/main.cpp)

target_include_directories(TaskQueue PRIVATE include)

# -----------------------------------------------------------------------
# Tests — GoogleTest will be added in Milestone 7 via FetchContent.
# The subdirectory is declared now so the structure is clear from the start.
# -----------------------------------------------------------------------
# add_subdirectory(tests)   ← will be uncommented in Milestone 7

# -----------------------------------------------------------------------
# Threading — required for std::thread, std::mutex, std::condition_variable
# -----------------------------------------------------------------------
find_package(Threads REQUIRED)
target_link_libraries(TaskQueue PRIVATE Threads::Threads)