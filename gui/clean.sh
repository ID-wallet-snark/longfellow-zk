#!/bin/bash
# Clean script for Longfellow ZK GUI
# Removes all generated files (build artifacts, CMake cache, binaries)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "🧹 Cleaning Longfellow ZK build artifacts..."
echo ""

# Function to remove files/directories safely
safe_remove() {
    if [ -e "$1" ]; then
        echo "  Removing: $1"
        rm -rf "$1"
    fi
}

# Clean root directory
cd "$PROJECT_ROOT"
echo "📁 Cleaning root directory..."
safe_remove CMakeCache.txt
safe_remove CMakeFiles
safe_remove cmake_install.cmake
safe_remove Makefile
safe_remove CTestTestfile.cmake

# Clean gui directory
echo ""
echo "🖥️  Cleaning GUI directory..."
cd "$SCRIPT_DIR"
safe_remove CMakeCache.txt
safe_remove CMakeFiles
safe_remove cmake_install.cmake
safe_remove Makefile
safe_remove longfellow_gui
safe_remove test_circuit
safe_remove imgui.ini

# Clean exported proofs
echo ""
echo "📄 Cleaning exported proofs..."
rm -f zkproof_*.json 2>/dev/null || true

# Clean lib directory
echo ""
echo "📚 Cleaning lib directory..."
cd "$PROJECT_ROOT/lib"
safe_remove CMakeCache.txt
safe_remove CMakeFiles
safe_remove cmake_install.cmake
safe_remove Makefile
safe_remove CTestTestfile.cmake

# Clean all subdirectories in lib
echo ""
echo "🔧 Cleaning library build artifacts..."
find . -type f -name "*.a" -delete 2>/dev/null || true
find . -type f -name "*.o" -delete 2>/dev/null || true
find . -type d -name "CMakeFiles" -exec rm -rf {} + 2>/dev/null || true
find . -type f -name "CMakeCache.txt" -delete 2>/dev/null || true
find . -type f -name "cmake_install.cmake" -delete 2>/dev/null || true
find . -type f -name "Makefile" -delete 2>/dev/null || true
find . -type f -name "*_include.cmake" -delete 2>/dev/null || true
find . -type f -name "CTestTestfile.cmake" -delete 2>/dev/null || true

# Clean test executables
echo ""
echo "🧪 Cleaning test executables..."
cd "$PROJECT_ROOT/lib"
find . -type f -executable -name "*_test" -delete 2>/dev/null || true

echo ""
echo "✅ Clean complete!"
echo ""
echo "To rebuild:"
echo "  cd $PROJECT_ROOT"
echo "  cmake ."
echo "  make longfellow_gui"
