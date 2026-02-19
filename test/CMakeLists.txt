cmake_minimum_required(VERSION 3.15)

# Find Google Test
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Test executable
add_executable(move_optimizer_tests
    test_main.cpp
)

target_compile_features(move_optimizer_tests PRIVATE cxx_std_17)
target_compile_definitions(move_optimizer_tests PRIVATE
    MOVE_OPTIMIZER_BIN="$<TARGET_FILE:move-optimizer>"
)

if(TARGET GTest::gtest)
    target_link_libraries(move_optimizer_tests
        GTest::gtest
        GTest::gtest_main
    )
else()
    target_link_libraries(move_optimizer_tests
        gtest
        gtest_main
    )
endif()

# Link against the main library if needed
# For now, tests will be integration tests using the compiled binary

enable_testing()
add_test(NAME MoveOptimizerTests COMMAND move_optimizer_tests)
