cmake_minimum_required(VERSION 3.30)
project(celtesystems)

set(CMAKE_CXX_STANDARD 20)

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

include_directories(include ${GLIB_INCLUDE_DIRS} ${RDKAFKA_INCLUDE_DIRS})

# Add pkg-config flags for glib-2.0
link_directories(${GLIB_LIBRARY_DIRS})
add_definitions(${GLIB_CFLAGS_OTHER})

set(KLSOURCES
    src/states/KLDisconnected.cpp
    src/states/KLErrorCouldNotConnect.cpp
    src/states/KLConnected.cpp
    src/KafkaFSM.cpp
)
add_library(KafkaLink ${KLSOURCES})
target_link_libraries(KafkaLink PRIVATE RdKafka::rdkafka RdKafka::rdkafka++ ${GLIB_LIBRARIES})

install(TARGETS KafkaLink
    LIBRARY DESTINATION celte-libraries
    PUBLIC_HEADER DESTINATION include
)