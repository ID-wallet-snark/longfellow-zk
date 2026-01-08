# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.13)
project(proofs)
set(CMAKE_CXX_STANDARD 17)
# include(CMake/proofs.cmake)
# Replacing with inlined content to remove BENCHMARK dependency
# include(CMake/proofs.cmake)
# Replacing with inlined content to remove BENCHMARK dependency
add_compile_definitions(OPENSSL_SUPPRESS_DEPRECATED=1)
# include(GoogleTest)
# find_package(GTest REQUIRED)

macro(proofs_add_testing_libraries PROG)
    # target_link_libraries(${PROG} testing_main)    
    # target_link_libraries(${PROG} gtest pthread)
    # gtest_discover_tests(${PROG})
endmacro()

macro(proofs_add_test PROG)
    # add_executable(${PROG} ${PROG}.cc ${ARGN})
    # target_link_libraries(${PROG} ec)    
    # target_link_libraries(${PROG} algebra)
    # target_link_libraries(${PROG} util)
    # proofs_add_testing_libraries(${PROG})
endmacro()

macro(proofs_add_tests)
    # foreach (PROG ${ARGN})
    #     proofs_add_test(${PROG})
    # endforeach ()
endmacro()

# enable_testing()
# Temporarily disable Homebrew includes to avoid conflicts with FetchContent libraries
# But keep link_directories for OpenSSL and other libraries
if (CMAKE_HOST_SYSTEM_NAME MATCHES "Darwin")
  link_directories("/opt/homebrew/lib")
#   include_directories("/opt/homebrew/include")
  # Add specific includes for dependencies (needed for crypto and compression)
  include_directories("/opt/homebrew/opt/openssl@3/include")
  include_directories("/opt/homebrew/opt/zstd/include")
endif()

# add compiler flags for all known ISA's
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCHITECTURE)

if(ARCHITECTURE MATCHES "x86_64")
    set(ARCH_FLAGS -mpclmul)
elseif(ARCHITECTURE MATCHES "i386")
    set(ARCH_FLAGS -msse2 -mpclmul)
elseif(ARCHITECTURE MATCHES "aarch64")
    set(ARCH_FLAGS -march=armv8-a -march=armv8-a+crypto)
elseif(ARCHITECTURE MATCHES "armv7l")
    set(ARCH_FLAGS -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard)
elseif(ARCHITECTURE MATCHES "arm64")
    # Mac Arm
    set(ARCH_FLAGS -mcpu=apple-m1 -march=armv8-a+crypto)
else()
    set(ARCH_FLAGS "")
endif()

if(ARCH_FLAGS)
    add_compile_options( "$<$<COMPILE_LANGUAGE:CXX>:${ARCH_FLAGS}>")
    message(STATUS "Compiler flags added for architecture ${ARCHITECTURE}: ${ARCH_FLAGS}")
else()
     message(STATUS "Architecture ${ARCHITECTURE} not recognized, no specific compiler flags added.")
endif()




include_directories(${proofs_SOURCE_DIR})

# Download GoogleTest and Benchmark via FetchContent
include(FetchContent)

# Download GoogleTest via FetchContent (needed for testing_main)
message(STATUS "Downloading GoogleTest via FetchContent...")
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "Disable GMock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Disable GTest install" FORCE)
FetchContent_MakeAvailable(googletest)

# Download Google Benchmark via FetchContent (needed by testing and mdoc)
message(STATUS "Downloading Google Benchmark via FetchContent...")
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.7.1
    GIT_SHALLOW TRUE
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Disable gtest tests" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "Disable benchmark install" FORCE)
# Disable warnings as errors for benchmark build
set(CMAKE_CXX_FLAGS_BACKUP "${CMAKE_CXX_FLAGS}")
string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
FetchContent_MakeAvailable(benchmark)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_BACKUP}")

add_subdirectory(testing)
add_subdirectory(util)
add_subdirectory(algebra)
add_subdirectory(arrays)
# add_subdirectory(merkle)
# add_subdirectory(ligero)
# add_subdirectory(proto)
# add_subdirectory(random)
# add_subdirectory(sumcheck)
# add_subdirectory(gf2k)
# add_subdirectory(cbor)
add_subdirectory(ec)
add_subdirectory(zk)
add_subdirectory(circuits/anoncred)
# add_subdirectory(circuits/base64)
# add_subdirectory(circuits/cbor_parser)
# add_subdirectory(circuits/compiler)
# add_subdirectory(circuits/ecdsa)
# add_subdirectory(circuits/jwt)
add_subdirectory(circuits/logic)
# add_subdirectory(circuits/mac)
add_subdirectory(circuits/mdoc)
add_subdirectory(circuits/sha)
# add_subdirectory(circuits/sha3)
