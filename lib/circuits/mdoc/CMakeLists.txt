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
# include(../../CMake/proofs.cmake)
# Inline fix:
add_compile_definitions(OPENSSL_SUPPRESS_DEPRECATED=1)
# include(GoogleTest)
# find_package(GTest REQUIRED)

macro(proofs_add_testing_libraries PROG)
endmacro()

macro(proofs_add_test PROG)
endmacro()

macro(proofs_add_tests)
endmacro()

add_library(mdoc mdoc_zk.cc mdoc_decompress.cc mdoc_generate_circuit.cc
                 mdoc_circuit_id.cc zk_spec.cc)
target_link_libraries(mdoc flatsha ec algebra util zstd)

add_library(mdoc_static STATIC
                        mdoc_zk.cc mdoc_decompress.cc mdoc_generate_circuit.cc
                        mdoc_circuit_id.cc zk_spec.cc
    $<TARGET_OBJECTS:flatsha>
    $<TARGET_OBJECTS:ec>
    $<TARGET_OBJECTS:algebra>
    $<TARGET_OBJECTS:util>
)
target_include_directories(mdoc_static PUBLIC ../../)

# proofs_add_test(mdoc_signature_test)
# target_link_libraries(mdoc_signature_test mdoc)

#proofs_add_test(zk_spec_test)
#target_link_libraries(zk_spec_test mdoc)

# proofs_add_test(mdoc_1f_test)
# target_link_libraries(mdoc_1f_test mdoc)

# link mdoc_zk_test explicitly against the static library
# so that we know that the static library works
add_executable(mdoc_zk_test mdoc_zk_test.cc)
target_link_libraries(mdoc_zk_test mdoc_static)
# proofs_add_testing_libraries(mdoc_zk_test)
target_link_libraries(mdoc_zk_test crypto zstd)

# add_executable(french_license_test french_license_test.cc)
# target_link_libraries(french_license_test mdoc_static)
# proofs_add_testing_libraries(french_license_test)
# target_link_libraries(french_license_test crypto zstd)

# add_executable(nationality_test nationality_test.cc)
# target_link_libraries(nationality_test mdoc_static)
# proofs_add_testing_libraries(nationality_test)
# target_link_libraries(nationality_test crypto zstd)

# Link against static lib
add_executable(student_verifier student_verifier.cc)
target_link_libraries(student_verifier mdoc_static)
# proofs_add_testing_libraries(student_verifier) # No GTest
target_link_libraries(student_verifier crypto zstd)


add_executable(student_card_test student_card_test.cc)
target_link_libraries(student_card_test mdoc_static)
proofs_add_testing_libraries(student_card_test)
target_link_libraries(student_card_test crypto zstd)

set(installable_libs mdoc_static)
install(TARGETS ${installable_libs} DESTINATION lib)
install(FILES mdoc_zk.h DESTINATION include)

find_package(absl QUIET)
if (absl_FOUND)
    message(STATUS "ABSL found. Adding circuit_maker.")
    add_executable(circuit_maker circuit_maker.cc)
    target_link_libraries(circuit_maker mdoc_static absl::flags absl::flags_parse absl::cleanup crypto zstd)
else()
    message(STATUS "ABSL not found. circuit_maker will not be built.")
    message(STATUS "ABSL not found. circuit_maker will not be built.")
endif()


add_executable(gentestmdoc gentestmdoc.cc)
target_link_libraries(gentestmdoc mdoc_static crypto zstd)
set_target_properties(gentestmdoc PROPERTIES CXX_STANDARD 17)

# Google Benchmark is now configured in root CMakeLists.txt

add_executable(mdoc_benchmark mdoc_benchmark.cc)
# Note: gtest is provided via testing_main
target_link_libraries(mdoc_benchmark mdoc_static testing_main benchmark::benchmark crypto zstd)
set_target_properties(mdoc_benchmark PROPERTIES CXX_STANDARD 17)

# Separate benchmark executables for different mDoc attributes
add_executable(age_over_18_benchmark age_over_18_benchmark.cc)
target_link_libraries(age_over_18_benchmark mdoc_static testing_main benchmark::benchmark crypto zstd)
set_target_properties(age_over_18_benchmark PROPERTIES CXX_STANDARD 17)

add_executable(height_benchmark height_benchmark.cc)
target_link_libraries(height_benchmark mdoc_static testing_main benchmark::benchmark crypto zstd)
set_target_properties(height_benchmark PROPERTIES CXX_STANDARD 17)

add_executable(birthdate_benchmark birthdate_benchmark.cc)
target_link_libraries(birthdate_benchmark mdoc_static testing_main benchmark::benchmark crypto zstd)
set_target_properties(birthdate_benchmark PROPERTIES CXX_STANDARD 17)

add_executable(driving_license_benchmark driving_license_benchmark.cc)
target_link_libraries(driving_license_benchmark mdoc_static testing_main benchmark::benchmark crypto zstd)
set_target_properties(driving_license_benchmark PROPERTIES CXX_STANDARD 17)

# Multi-attribute scalability benchmark
add_executable(multi_attribute_benchmark multi_attribute_benchmark.cc)
target_link_libraries(multi_attribute_benchmark mdoc_static testing_main benchmark::benchmark crypto zstd)
set_target_properties(multi_attribute_benchmark PROPERTIES CXX_STANDARD 17)

# Multi-document comparison benchmark
add_executable(multi_document_benchmark multi_document_benchmark.cc)
target_link_libraries(multi_document_benchmark mdoc_static testing_main benchmark::benchmark crypto zstd)
set_target_properties(multi_document_benchmark PROPERTIES CXX_STANDARD 17)

# Memory profiling benchmark
add_executable(memory_benchmark memory_benchmark.cc)
target_link_libraries(memory_benchmark mdoc_static testing_main benchmark::benchmark crypto zstd)
set_target_properties(memory_benchmark PROPERTIES CXX_STANDARD 17)

# Student card benchmark
add_executable(student_card_bench student_card_bench.cc)
target_link_libraries(student_card_bench mdoc_static testing_main benchmark::benchmark crypto zstd)
set_target_properties(student_card_bench PROPERTIES CXX_STANDARD 17)











