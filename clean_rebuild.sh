#!/usr/bin/env bash
set -e

echo "Cleaning build directory..."
rm -rf build
mkdir build
cd build

echo "Configuring with CMake..."
cmake .. -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DBENCHMARK_ENABLE_WERROR=OFF

echo "Building..."
ninja mdoc_benchmark smart_age_test age_over_18_benchmark

echo "Running smart_age_test..."
./lib/circuits/mdoc/smart_age_test

echo "Running mdoc_benchmark..."
./lib/circuits/mdoc/mdoc_benchmark --benchmark_filter=SmartAge

echo "Running age_over_18_benchmark..."
./lib/circuits/mdoc/age_over_18_benchmark
