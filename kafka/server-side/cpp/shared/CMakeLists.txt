cmake_minimum_required(VERSION 3.10)
project(CelteSharedUtilities)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------- DISABLE IN SOURCE BUILD ------------------------ */

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
message(FATAL_ERROR "In-source builds are not allowed. Please run cmake from the 'build' directory.")
endif()



include_directories(include)

add_library(CelteSharedUtilities SHARED
    src/Config.cpp)