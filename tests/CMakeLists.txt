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


# Set the source files for the unit tests
set(SOURCES
    test_runtime_connected_cluster.cpp
)

add_executable(test_server ${SOURCES})

target_link_libraries(test_server gtest gtest_main)
target_link_libraries(test_server celtesystems_runtime_server)