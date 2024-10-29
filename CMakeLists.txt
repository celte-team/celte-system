cmake_minimum_required(VERSION 3.27)
project(celte-systems)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# Set default installation prefix
set( "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation prefix")

# Check for in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed. Please create a separate build directory and run CMake from there.")
endif()

# Include directories
include_directories(
    # kafka/cpp/include
    client/include
    server/include
    runtime/include)

include_directories(${CMAKE_PREFIX_PATH}/include)

# here
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")
set(INSTALL_LIBRARY_DIR "${CMAKE_INSTALL_PREFIX}/lib")

message(STATUS "Headers will be installed to ${INSTALL_INCLUDE_DIR}")
message(STATUS "Libraries will be installed to ${INSTALL_LIBRARY_DIR}")

# Add subdirectories
add_subdirectory(runtime)
add_subdirectory(tests)
add_subdirectory(tests/single_node_poc)
add_subdirectory(tests/test_authority_transfer)

# Find and configure RdKafka
find_package(RdKafka CONFIG REQUIRED)



# Include directories for librdkafka
include_directories("build/vcpkg_installed/x64-linux/include/librdkafka")

# For debugging purposes, print include directories
message(STATUS "RDKAFKA_INCLUDE_DIRS: ${RDKAFKA_INCLUDE_DIRS}")
message(STATUS "RDKAFKA_LIBRARY: ${RDKAFKA_LIBRARY}")

# Install RdKafka libraries
install(FILES ${RDKAFKA_LIBRARY} ${RDKAFKA++_LIBRARY}
        DESTINATION celte-libraries)

# Find and install RdKafka libraries
find_library(RDKAFKA_LIBRARY NAMES rdkafka HINTS ${CMAKE_PREFIX_PATH}/lib)
find_library(RDKAFKA++_LIBRARY NAMES rdkafka++ HINTS ${CMAKE_PREFIX_PATH}/lib)

# Ensure the libraries are found
if(NOT RDKAFKA_LIBRARY)
    message(FATAL_ERROR "rdkafka library not found")
endif()
if(NOT RDKAFKA++_LIBRARY)
    message(FATAL_ERROR "rdkafka++ library not found")
endif()
# Install header files

message(STATUS "Installing headers")
if (APPLE)
    # Include directories for librdkafka
    install(DIRECTORY "build/vcpkg_installed/arm64-osx/include/librdkafka" DESTINATION ${INSTALL_INCLUDE_DIR})
    install(DIRECTORY "build/vcpkg_installed/arm64-osx/include/glm" DESTINATION ${INSTALL_INCLUDE_DIR})
    install(DIRECTORY "build/vcpkg_installed/arm64-osx/include/msgpack" DESTINATION ${INSTALL_INCLUDE_DIR})
    install(FILES "build/vcpkg_installed/arm64-osx/include/msgpack.hpp" DESTINATION ${INSTALL_INCLUDE_DIR})
else()
    # Include directories for librdkafka
    install(DIRECTORY "build/vcpkg_installed/x64-linux/include/librdkafka" DESTINATION ${INSTALL_INCLUDE_DIR})
    install(DIRECTORY "build/vcpkg_installed/x64-linux/include/glm" DESTINATION ${INSTALL_INCLUDE_DIR})
    install(DIRECTORY "build/vcpkg_installed/x64-linux/include/msgpack" DESTINATION ${INSTALL_INCLUDE_DIR})
    install(FILES "build/vcpkg_installed/x64-linux/include/msgpack.hpp" DESTINATION ${INSTALL_INCLUDE_DIR})
endif()

install(DIRECTORY client/include/ DESTINATION ${INSTALL_INCLUDE_DIR}/client)
install(DIRECTORY server/include/ DESTINATION ${INSTALL_INCLUDE_DIR}/server)
install(DIRECTORY runtime/include/ DESTINATION ${INSTALL_INCLUDE_DIR}/runtime)
