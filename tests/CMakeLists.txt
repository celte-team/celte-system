cmake_minimum_required(VERSION 3.12)
project(celte-unit-tests)

include(FetchContent)

# Fetch GoogleTest and GoogleMock
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.11.0
)

FetchContent_MakeAvailable(googletest)

enable_testing()

find_package(RdKafka CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(msgpack CONFIG REQUIRED NAMES msgpack-cxx)

pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(include
    ${GLIB_INCLUDE_DIRS}
    ${RDKAFKA_INCLUDE_DIRS}
)

# Set the source files for the unit tests
set(SOURCES
    test_clock.cpp  # Add your new test file here
)

add_executable(test_server ${SOURCES})
target_compile_definitions(test_server PRIVATE -DCELTE_SERVER_MODE_ENABLED)
target_link_libraries(test_server gtest gmock gtest_main msgpack-cxx)
target_link_libraries(test_server celtesystems_runtime_server)