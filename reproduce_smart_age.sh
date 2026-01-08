#!/bin/bash
set -e

# Setup build directory
mkdir -p build
cd build

# Run CMake and Build
# Assuming the root CMakeLists.txt is at project root, which is 4 levels up from this script location relative to lib/circuits/mdoc
# BUT, we are running this script from project root /Users/anselme/Documents/projet-ID/longfellow-zk
cmake . 

# Build only the relevant target to save time
cmake --build . --target smart_age_test

# Run the test
# The binary should be in lib/circuits/mdoc/
./lib/circuits/mdoc/smart_age_test
