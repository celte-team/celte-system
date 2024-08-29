cmake_minimum_required(VERSION 3.12)
project(celtekafka)

set(CMAKE_CXX_STANDARD 20)

if (DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
else ()
    message(FATAL_ERROR "Please define VCPKG_ROOT environment variable")
endif ()

# Set CMAKE_PREFIX_PATH to include vcpkg for macOS
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};$ENV{VCPKG_ROOT}/installed/arm64-osx")

find_package(RdKafka CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(include ${GLIB_INCLUDE_DIRS})

# Add pkg-config flags for glib-2.0
link_directories(${GLIB_LIBRARY_DIRS})
add_definitions(${GLIB_CFLAGS_OTHER})

add_executable(consumer-cpp src/consumer.cpp)
target_link_libraries(consumer-cpp PRIVATE RdKafka::rdkafka RdKafka::rdkafka++ ${GLIB_LIBRARIES})

add_executable(producer-cpp src/producer.cpp)
target_link_libraries(producer-cpp PRIVATE RdKafka::rdkafka RdKafka::rdkafka++ ${GLIB_LIBRARIES})