cmake_minimum_required(VERSION 3.30)
project(celtesystems)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(RdKafka CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(include)

# Add pkg-config flags for glib-2.0
link_directories(${GLIB_LIBRARY_DIRS})
add_definitions(${GLIB_CFLAGS_OTHER})

set(COMMON_SOURCES
    src/CelteRuntime.cpp
)

# Runtime for server
add_library(celtesystems_runtime_server SHARED ${COMMON_SOURCES})
target_compile_definitions(celtesystems_runtime_server PRIVATE -DCELTE_SERVER_MODE_ENABLED)
target_link_libraries(celtesystems_runtime_server PRIVATE KafkaLink CelteServer)

# # Runtime for client
add_library(celtesystems_runtime_client SHARED ${COMMON_SOURCES})
target_link_libraries(celtesystems_runtime_client PRIVATE KafkaLink CelteClient)





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

install(FILES ${RDKAFKA_LIBRARY} ${RDKAFKA++_LIBRARY}
        DESTINATION celte-libraries)

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

# Ensure the libraries are found
if(NOT BOOST_SYSTEM_LIBRARY)
    message(FATAL_ERROR "boost_system library not found")
endif()

if(NOT BOOST_PROGRAM_OPTIONS_LIBRARY)
    message(FATAL_ERROR "boost_program_options library not found")
endif()

install(FILES ${BOOST_SYSTEM_LIBRARY} ${BOOST_PROGRAM_OPTIONS_LIBRARY}
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
