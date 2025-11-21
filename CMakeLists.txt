cmake_minimum_required(VERSION 3.13)
project(LongfellowZK VERSION 1.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the core libraries first
add_subdirectory(lib)

# Build the GUI application
# add_subdirectory(gui)
