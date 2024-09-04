cmake_minimum_required(VERSION 3.30)
project(celte-systems)

set(CMAKE_CXX_STANDARD 20)

# Set default installation prefix
set( "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation prefix")

# Check for in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed. Please create a separate build directory and run CMake from there.")
endif()

# Include directories
include_directories(
    kafka/cpp/include
    client/include
    server/include
    runtime/include)

include_directories(${CMAKE_PREFIX_PATH}/include)

set(INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")
set(INSTALL_LIBRARY_DIR "${CMAKE_INSTALL_PREFIX}/lib")

message(STATUS "Headers will be installed to ${INSTALL_INCLUDE_DIR}")
message(STATUS "Libraries will be installed to ${INSTALL_LIBRARY_DIR}")

# Add subdirectories
add_subdirectory(kafka/cpp)
add_subdirectory(client)
add_subdirectory(server)
add_subdirectory(runtime)
add_subdirectory(tests)


# Install header files
install(DIRECTORY kafka/cpp/include/ DESTINATION ${INSTALL_INCLUDE_DIR}/kafka/cpp)
install(DIRECTORY client/include/ DESTINATION ${INSTALL_INCLUDE_DIR}/client)
install(DIRECTORY server/include/ DESTINATION ${INSTALL_INCLUDE_DIR}/server)
install(DIRECTORY runtime/include/ DESTINATION ${INSTALL_INCLUDE_DIR}/runtime)
