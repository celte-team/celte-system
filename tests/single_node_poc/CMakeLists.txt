cmake_minimum_required(VERSION 3.12)
project(celte-functional-tests)

include(FetchContent)


FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.11.0
)

enable_testing()

FetchContent_MakeAvailable(googletest)


if (DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
else ()
    message(FATAL_ERROR "Please define VCPKG_ROOT environment variable")
endif ()

if(APPLE)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};$ENV{VCPKG_ROOT}/installed/arm64-osx")
elseif(UNIX)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};$ENV{VCPKG_ROOT}/installed/arm64-linux")
elseif(WIN32)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};$ENV{VCPKG_ROOT}/installed/x64-windows")
endif()

find_package(RdKafka CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(include
    ${GLIB_INCLUDE_DIRS}
    ${RDKAFKA_INCLUDE_DIRS}
)


# Set the source files for the functional tests
set(SOURCES
    test_server_single_node.cpp
)

add_executable(test_server_single_node ${SOURCES})
target_compile_definitions(test_server_single_node PRIVATE -DCELTE_SERVER_MODE_ENABLED)
target_link_libraries(test_server_single_node gtest gtest_main)
target_link_libraries(test_server_single_node celtesystems_runtime_server)

set(SOURCES_SINGLE_NODE_CLIENT
    test_client_single_node.cpp
)

add_executable(test_client_single_node ${SOURCES_SINGLE_NODE_CLIENT})
target_link_libraries(test_client_single_node gtest gtest_main)
target_link_libraries(test_client_single_node celtesystems_runtime_client)
