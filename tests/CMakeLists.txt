cmake_minimum_required(VERSION 3.12)
project(celte-unit-tests)

include(FetchContent)


FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.11.0
)

enable_testing()

FetchContent_MakeAvailable(googletest)

add_subdirectory(single_node_poc)
# here
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(RdKafka CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(include
    ${GLIB_INCLUDE_DIRS}
    ${RDKAFKA_INCLUDE_DIRS}
)


# Set the source files for the unit tests
set(SOURCES
    test_runtime_connected_cluster.cpp
)

add_executable(test_server ${SOURCES})
target_compile_definitions(test_server PRIVATE -DCELTE_SERVER_MODE_ENABLED)
target_link_libraries(test_server gtest gtest_main)
target_link_libraries(test_server celtesystems_runtime_server)