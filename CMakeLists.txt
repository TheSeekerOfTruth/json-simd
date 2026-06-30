cmake_minimum_required(VERSION 3.16)
project(JsonSimdMinifier CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O3 -march=native -mpclmul -mavx512f -mavx512bw -mavx512vbmi2)
endif()

include_directories(include)

# --- 1. Fetch Google Benchmark ---
include(FetchContent)
FetchContent_Declare(
  google_benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.9.1
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(google_benchmark)

# --- 2. Advanced Multiarch Detection for Linux libjq ---
find_path(JQ_INCLUDE_DIR 
    NAMES jq.h
    PATHS 
    /usr/include
    /usr/include/x86_64-linux-gnu
    /usr/local/include
)

find_library(JQ_LIB 
    NAMES jq
    PATHS 
    /usr/lib
    /usr/lib/x86_64-linux-gnu
    /usr/local/lib
)

if(JQ_LIB AND JQ_INCLUDE_DIR)
    message(STATUS "SUCCESS: Found libjq header at: ${JQ_INCLUDE_DIR}")
    message(STATUS "SUCCESS: Found libjq binary at: ${JQ_LIB}")
    include_directories(${JQ_INCLUDE_DIR})
else()
    message(WARNING "CRITICAL: libjq or jq.h NOT found. JQ benchmarks will be skipped.")
endif()

# --- 3. Build Targets ---
add_executable(simd_minifier 
    src/simd_minify.cpp
    src/simd_minify_nibble.cpp
    src/main.cpp
)

# Benchmark Target
add_executable(minify_benchmark 
    benchmarks/benchmark_minify.cpp
    src/simd_minify.cpp
    src/simd_minify_nibble.cpp
)
target_link_libraries(minify_benchmark benchmark::benchmark)

if(JQ_LIB AND JQ_INCLUDE_DIR)
    target_link_libraries(minify_benchmark ${JQ_LIB})
    target_compile_definitions(minify_benchmark PRIVATE HAS_JQ=1)
endif()

# --- 4. Streamlined Verification Test Target ---
add_executable(verify_outputs
    tests/verify_outputs.cpp
    src/simd_minify.cpp
    src/simd_minify_nibble.cpp
)