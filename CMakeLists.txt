cmake_minimum_required(VERSION 3.16)
project(JsonSimdMinifier CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force high-optimization vectors and native microarchitecture features (AVX-512, CLMUL)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-O3 -march=native -mpclmul -mavx512f -mavx512bw -mavx512vbmi2)
endif()

# Setup Include Paths
include_directories(include)

# Define target and all associated source nodes
add_executable(simd_minifier 
    src/simd_minify.cpp
    src/main.cpp
)