cmake_minimum_required(VERSION 3.10)
project(LongfellowGUI VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Set OpenGL preference to GLVND
set(OpenGL_GL_PREFERENCE GLVND)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(GLEW REQUIRED glew)

# ImGui source files
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUI_SOURCES
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Create GUI executable
add_executable(longfellow_gui
  main.cpp
  ${IMGUI_SOURCES}
)

# Include directories
target_include_directories(longfellow_gui PRIVATE
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${GLFW_INCLUDE_DIRS}
  ${GLEW_INCLUDE_DIRS}
  ${OPENGL_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/lib
  ${CMAKE_SOURCE_DIR}/lib/circuits/mdoc
)

# Link libraries
target_link_libraries(longfellow_gui PRIVATE
  ${GLFW_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES}
  mdoc
  flatsha
  ec
  algebra
  util
  crypto
  zstd
  dl
  pthread
)

# Test executable for debugging circuit generation
add_executable(test_circuit test_circuit.cpp)
target_include_directories(test_circuit PRIVATE
  ${CMAKE_SOURCE_DIR}/lib
  ${CMAKE_SOURCE_DIR}/lib/circuits/mdoc
)
target_link_libraries(test_circuit PRIVATE
  mdoc
  flatsha
  ec
  algebra
  util
  crypto
  zstd
  dl
  pthread
)
