cmake_minimum_required(VERSION 3.12)
project(celtesystems)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)


if(APPLE)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};$ENV{VCPKG_ROOT}/installed/arm64-osx")
elseif(UNIX)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};$ENV{VCPKG_ROOT}/installed/arm64-linux")
elseif(WIN32)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};$ENV{VCPKG_ROOT}/installed/x64-windows")
endif()

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")


if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed. Please create a separate build directory and run CMake from there.")
endif()

include_directories(
    kafka/cpp/include
    client/include
    server/include)

add_subdirectory(runtime)
add_subdirectory(kafka/cpp)
add_subdirectory(client)
add_subdirectory(server)
