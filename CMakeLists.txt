cmake_minimum_required(VERSION 3.20)

project(
    event_bus
    VERSION 1.0.0
    DESCRIPTION "Lock-free MPMC EventBus in Modern C++"
    LANGUAGES CXX
)

# ----------------------------------
# Global settings
# ----------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ----------------------------------
# Compiler warnings (professional)
# ----------------------------------
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
    )
endif()

# ----------------------------------
# Library: event_bus
# ----------------------------------
add_library(event_bus STATIC
    src/event_bus.cpp
)

target_include_directories(event_bus
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(event_bus PUBLIC cxx_std_20)

# ----------------------------------
# Threading
# ----------------------------------
find_package(Threads REQUIRED)
target_link_libraries(event_bus PUBLIC Threads::Threads)

# ----------------------------------
# Benchmark executables
# ----------------------------------
add_executable(benchmark
    benchmarks/benchmark.cpp
)

target_link_libraries(benchmark
    PRIVATE event_bus
)

add_executable(benchmark_graph
    benchmarks/benchmark_graph.cpp
)

target_link_libraries(benchmark_graph
    PRIVATE event_bus
)

# ----------------------------------
# Optimizations for benchmarks
# ----------------------------------
if(NOT MSVC)
    target_compile_options(benchmark PRIVATE -O3)
    target_compile_options(benchmark_graph PRIVATE -O3)
else()
    target_compile_options(benchmark PRIVATE /O2)
    target_compile_options(benchmark_graph PRIVATE /O2)
endif()

# ----------------------------------
# Output directories (clean)
# ----------------------------------
set_target_properties(benchmark benchmark_graph PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(event_bus PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# ----------------------------------
# Install (optional, but professional)
# ----------------------------------
install(TARGETS event_bus
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)
