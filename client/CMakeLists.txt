cmake_minimum_required(VERSION 3.30)
project(celtesystems)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


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

add_library(CelteClient SHARED
    src/CelteClient.cpp
    src/StateConnected.cpp
    src/StateConnecting.cpp
    src/StateDisconnected.cpp
)

target_link_libraries(CelteClient PRIVATE KafkaLink  ${GLIB_LIBRARIES})

install(TARGETS CelteClient
    LIBRARY DESTINATION celte-libraries
    PUBLIC_HEADER DESTINATION include
)