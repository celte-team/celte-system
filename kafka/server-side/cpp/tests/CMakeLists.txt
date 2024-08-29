cmake_minimum_required(VERSION 3.12)

project(CelteTestSuite)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
include(CTest)

# Add the GoogleTest library
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.11.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

set(TEST_SOURCES
    test_nlSendRecv.cpp
)

set(TEST_LINK_LIBRARIES
    CelteNL
    CelteSharedUtilities
    gtest
    gtest_main
)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})
target_link_libraries(${PROJECT_NAME} ${TEST_LINK_LIBRARIES})

# Register the test executable with CTest
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})