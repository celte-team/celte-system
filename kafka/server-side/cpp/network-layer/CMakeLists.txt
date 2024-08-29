cmake_minimum_required(VERSION 3.5)
project(Celte)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
message(FATAL_ERROR "In-source builds are not allowed. Please run cmake from the 'build' directory.")
endif()

find_package(ZLIB REQUIRED)

set(SRC
    src/CelteNL.cpp
    src/Packet.cpp
    src/Connection.cpp
    src/NLUtils.cpp
)

add_library(CelteNL SHARED ${SRC})
target_link_libraries(CelteNL
    CelteSharedUtilities
    ${Boost_LIBRARIES}
    ZLIB::ZLIB)
