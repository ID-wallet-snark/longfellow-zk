#!/usr/bin/env bash
set -e

echo "Cleaning build directory..."
rm -rf build
mkdir build
cd build

echo "Configuring with CMake..."
cmake .. -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DBENCHMARK_ENABLE_WERROR=OFF

echo "Building EVERYTHING..."
ninja mdoc_signature_test mdoc_1f_test mdoc_zk_test \
      mdoc_benchmark smart_age_test age_over_18_benchmark \
      french_license_test sex_test student_card_test student_card_bench \
      ptrcred_age_over_18_test ptrcred_eu_residency_test

echo ">> Smart Age & License"

./lib/circuits/mdoc/mdoc_benchmark --benchmark_filter=SmartAge
./lib/circuits/mdoc/age_over_18_benchmark

./lib/circuits/anoncred/ptrcred_age_over_18_test
./lib/circuits/mdoc/smart_age_test
./lib/circuits/mdoc/french_license_test

echo ">> Gender & Student"
./lib/circuits/mdoc/sex_test
./lib/circuits/mdoc/student_card_test
./lib/circuits/mdoc/student_card_bench

echo ">> European Residency (AnonCreds)"
./lib/circuits/anoncred/ptrcred_eu_residency_test --benchmark_filter=EUResidency
