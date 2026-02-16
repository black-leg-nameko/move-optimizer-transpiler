#!/bin/bash
# Build script for Move Optimizer Transpiler

set -e

echo "=== Move Optimizer Transpiler Build Script ==="

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "Error: CMake is not installed."
    echo "Please install CMake: brew install cmake (macOS) or apt-get install cmake (Linux)"
    exit 1
fi

# Check for LLVM
if [ -z "$LLVM_DIR" ]; then
    # Try to find LLVM via Homebrew (macOS)
    if command -v brew &> /dev/null; then
        LLVM_PREFIX=$(brew --prefix llvm 2>/dev/null || echo "")
        if [ -n "$LLVM_PREFIX" ] && [ -d "$LLVM_PREFIX" ]; then
            export LLVM_DIR="$LLVM_PREFIX/lib/cmake/llvm"
            export Clang_DIR="$LLVM_PREFIX/lib/cmake/clang"
            echo "Found LLVM at: $LLVM_PREFIX"
        fi
    fi
fi

# Create build directory
BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

echo "=== Configuring with CMake ==="
cmake .. || {
    echo "CMake configuration failed."
    echo "Please ensure LLVM is installed and LLVM_DIR is set correctly."
    echo "For macOS with Homebrew: export LLVM_DIR=\$(brew --prefix llvm)/lib/cmake/llvm"
    exit 1
}

echo "=== Building ==="
cmake --build . || {
    echo "Build failed."
    exit 1
}

echo "=== Build completed successfully ==="
echo "Executable: $BUILD_DIR/move-optimizer"
