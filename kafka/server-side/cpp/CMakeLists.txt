cmake_minimum_required(VERSION 3.0)
project(Celte)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ------------------------- DISABLE IN SOURCE BUILD ------------------------ */
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
message(FATAL_ERROR "In-source builds are not allowed. Please run cmake from the 'build' directory.")
endif()


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(BOOST_COMPONENTS
system
thread
serialization
chrono
)

find_package(Boost 1.75 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
include_directories(
    ${Boost_INCLUDE_DIRS}
    shared/include
    network-layer/include
)




# -------------------------------- SUB DIRS -------------------------------- */

add_subdirectory(shared)
add_subdirectory(network-layer)
add_subdirectory(tests)
