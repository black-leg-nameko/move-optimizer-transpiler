cmake_minimum_required(VERSION 3.15)
project(MoveOptimizerTranspiler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

# Include directories
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(include)

# Add definitions
add_definitions(${LLVM_DEFINITIONS})
add_definitions(${CLANG_DEFINITIONS})

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_DEFINITIONS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

# Source files
set(SOURCES
    src/main.cpp
    src/move_optimizer.cpp
    src/ast_visitor.cpp
    src/code_transformer.cpp
)

set(HEADERS
    include/move_optimizer.h
    include/ast_visitor.h
    include/code_transformer.h
)

# Main executable
add_executable(move-optimizer ${SOURCES} ${HEADERS})

# Link libraries
llvm_map_components_to_libnames(LLVM_LIBS
    core
    support
)

target_link_libraries(move-optimizer
    ${LLVM_LIBS}
    clangTooling
    clangAST
    clangBasic
    clangFrontend
    clangRewrite
    clangToolingCore
)

# Test configuration
enable_testing()
add_subdirectory(test)
