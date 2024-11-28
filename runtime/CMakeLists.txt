cmake_minimum_required(VERSION 3.20)
project(celtesystems)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add the vcpkg toolchain file
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

find_package(RdKafka CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
find_package(glm CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread json)


include_directories(include ${GLIB_INCLUDE_DIRS} ${RDKAFKA_INCLUDE_DIRS})

# Add pkg-config flags for glib-2.0
link_directories(${GLIB_LIBRARY_DIRS})
add_definitions(${GLIB_CFLAGS_OTHER})

find_package(msgpack CONFIG REQUIRED NAMES msgpack-cxx)

set(COMMON_SOURCES
    src/CelteRuntime.cpp
    src/CelteEntity.cpp
    src/CelteGrape.cpp
    src/CelteChunk.cpp
    src/CelteRPC.cpp
    src/CelteGrapeManagementSystem.cpp
    src/CelteEntityManagementSystem.cpp
    src/CelteHooks.cpp
    src/RotatedBoundingBox.cpp
    src/CelteClock.cpp
    src/Replicator.cpp
    src/Logger.cpp
    src/KPool.cpp
    src/CelteInputSystem.cpp
    src/ConsumerWorker.cpp
    src/KConsumerService.cpp
)

set(SERVER_SOURCES
    ../server/src/CelteServer.cpp
    ../server/src/StateConnected.cpp
    ../server/src/StateConnecting.cpp
    ../server/src/StateDisconnected.cpp
)

set(CLIENT_SOURCES
    ../client/src/CelteClient.cpp
    ../client/src/StateConnected.cpp
    ../client/src/StateConnecting.cpp
    ../client/src/StateDisconnected.cpp
)

# Runtime for server
add_library(celtesystems_runtime_server SHARED ${COMMON_SOURCES} ${SERVER_SOURCES})
target_compile_definitions(celtesystems_runtime_server PRIVATE -DCELTE_SERVER_MODE_ENABLED)
target_link_libraries(celtesystems_runtime_server PRIVATE glm::glm msgpack-cxx Boost::thread Boost::json RdKafka::rdkafka RdKafka::rdkafka++ ${GLIB_LIBRARIES})

# Runtime for client
add_library(celtesystems_runtime_client SHARED ${COMMON_SOURCES} ${CLIENT_SOURCES})
target_link_libraries(celtesystems_runtime_client PRIVATE glm::glm msgpack-cxx Boost::thread Boost::json RdKafka::rdkafka RdKafka::rdkafka++ ${GLIB_LIBRARIES})

######################################################################
#  INSTALL DEPENDENCIES
######################################################################

# RDKafka

# Find RdKafka libraries
find_library(RDKAFKA_LIBRARY NAMES rdkafka HINTS ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib)
find_library(RDKAFKA++_LIBRARY NAMES rdkafka++ HINTS ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib)

# Ensure the libraries are found
if(NOT RDKAFKA_LIBRARY)
    message(FATAL_ERROR "rdkafka library not found")
endif()
if(NOT RDKAFKA++_LIBRARY)
    message(FATAL_ERROR "rdkafka++ library not found")
endif()

# install(FILES ${RDKAFKA_LIBRARY} ${RDKAFKA++_LIBRARY}
#         DESTINATION celte-libraries)

# Glib

# Find Glib libraries
find_library(GLIB_LIBRARY NAMES glib-2.0 HINTS ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib)

# Ensure the libraries are found
if(NOT GLIB_LIBRARY)
    message(FATAL_ERROR "glib-2.0 library not found")
endif()

install(FILES ${GLIB_LIBRARY}
        DESTINATION celte-libraries)

# Boost

# Find Boost libraries
find_library(BOOST_SYSTEM_LIBRARY NAMES boost_system HINTS ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib)
find_library(BOOST_PROGRAM_OPTIONS_LIBRARY NAMES boost_program_options HINTS ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib)
find_library(BOOST_THREAD_LIBRARY NAMES boost_thread HINTS ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib)
find_library(BOOST_JSON_LIBRARY NAMES boost_json HINTS ${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib)

# Ensure the libraries are found
if(NOT BOOST_SYSTEM_LIBRARY)
    message(FATAL_ERROR "boost_system library not found")
endif()

if(NOT BOOST_PROGRAM_OPTIONS_LIBRARY)
    message(FATAL_ERROR "boost_program_options library not found")
endif()

install(FILES ${BOOST_SYSTEM_LIBRARY} ${BOOST_PROGRAM_OPTIONS_LIBRARY}
        DESTINATION celte-libraries)
install(FILES ${BOOST_THREAD_LIBRARY} ${BOOST_JSON_LIBRARY}
    DESTINATION celte-libraries)

# CELTE Targets

install(TARGETS celtesystems_runtime_server celtesystems_runtime_client
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION celte-libraries
        ARCHIVE DESTINATION celte-libraries
        PUBLIC_HEADER DESTINATION include)

set(CPACK_PACKAGE_NAME "celtesystems")
set(CPACK_PACKAGE_VERSION "0.0.1")
set(CPACK_PACKAGE_CONTACT "celte-team.pro@gmail.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Celtesystems - Server meshing")
set(CPACK_PACKAGE_VENDOR "The Celte Team")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
set(CPACK_GENERATOR "TGZ;ZIP")

include(CPack)
