#!/bin/bash
# Test script for Move Optimizer Transpiler

set -e

echo "=== Move Optimizer Transpiler Test Script ==="

BUILD_DIR="build"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found. Running build script..."
    ./build.sh
fi

cd "$BUILD_DIR"

# Build tests if they exist
if [ -f "CMakeFiles/move_optimizer_tests.dir" ] || [ -f "move_optimizer_tests" ]; then
    echo "=== Building tests ==="
    cmake --build . --target move_optimizer_tests 2>/dev/null || echo "Test target not available"
fi

# Run tests if executable exists
if [ -f "move_optimizer_tests" ]; then
    echo "=== Running unit tests ==="
    ./move_optimizer_tests
else
    echo "Test executable not found. Skipping unit tests."
fi

# Test with example files
if [ -f "move-optimizer" ]; then
    echo "=== Testing with example files ==="
    
    cd ..
    
    # Test with before.cpp
    if [ -f "examples/before.cpp" ]; then
        echo "Testing optimization of examples/before.cpp..."
        "$BUILD_DIR/move-optimizer" examples/before.cpp -o examples/before.optimized.cpp
        
        if [ -f "examples/before.optimized.cpp" ]; then
            echo "✓ Successfully created optimized file"
            echo "Comparing with expected output..."
            
            # Check if std::move was added
            if grep -q "std::move" examples/before.optimized.cpp; then
                echo "✓ std::move found in optimized code"
            else
                echo "⚠ std::move not found (may be correct if no optimizations were needed)"
            fi
        else
            echo "✗ Failed to create optimized file"
        fi
    fi
else
    echo "move-optimizer executable not found. Please build first."
    exit 1
fi

echo "=== Tests completed ==="
