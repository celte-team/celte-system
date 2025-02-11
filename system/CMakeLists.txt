cmake_minimum_required(VERSION 3.10)
project(celte-systems-api LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${ENV}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
else()
    message(STATUS "ccache not found")
endif()

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

find_package(Protobuf REQUIRED)
find_package(TBB REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread system)
find_package(unofficial-pulsar CONFIG)
find_package(hiredis CONFIG REQUIRED)


# GENERATE PROTO FILES
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/protos/)
set(PROTOGEN_DIR ${CMAKE_SOURCE_DIR}/protogen)
set(PROTO_EXEC_PATH ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/protobuf)

file(GLOB PROTO_FILES "${PROTO_DIR}/*.proto")
message(STATUS "protoc executable: ${PROTO_EXEC_PATH}/protoc")
execute_process(
    COMMAND ${PROTO_EXEC_PATH}/protoc ${PROTO_FILES} --cpp_out=${PROTOGEN_DIR} --proto_path=${PROTO_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)

# Optionally, check if the command was successful
if(result)
    message(FATAL_ERROR "Error running protoc: ${error_output}")
else()
    message(STATUS "Protoc ran successfully: ${output}")
endif()



set(PROTO_SRC_ROOT "${CMAKE_SOURCE_DIR}/protogen/")
set(PROTO_SRCS
    "${PROTO_SRC_ROOT}/systems_structs.pb.cc"
)

file(GLOB_RECURSE SOURCES "common_src/*.cpp")
include_directories("${CMAKE_SOURCE_DIR}/include/" "${CMAKE_SOURCE_DIR}/protogen")

file(GLOB_RECURSE CLIENT_SRC "${CMAKE_SOURCE_DIR}/client_src/*.cpp")
file(GLOB_RECURSE SERVER_SRC "${CMAKE_SOURCE_DIR}/server_src/*.cpp")

set(LINK_LIBS_SYSTEMS_API
    ${Protobuf_LIBRARIES}
    TBB::tbb
    TBB::tbbmalloc
    Boost::system
    Boost::thread
    unofficial::pulsar::pulsar
    protobuf::libprotoc
    protobuf::libprotobuf
    protobuf::libprotobuf-lite
    hiredis::hiredis
)

add_library(${PROJECT_NAME}_server SHARED ${SOURCES} ${SERVER_SRC} ${PROTO_SRCS})
target_compile_definitions(${PROJECT_NAME}_server PRIVATE CELTE_SERVER_MODE_ENABLED)
target_link_libraries(${PROJECT_NAME}_server ${LINK_LIBS_SYSTEMS_API})

add_library(${PROJECT_NAME}_client SHARED ${SOURCES} ${CLIENT_SRC} ${PROTO_SRCS})
target_compile_definitions(${PROJECT_NAME}_client PRIVATE CELTE_CLIENT_MODE_ENABLED)
target_link_libraries(${PROJECT_NAME}_client ${LINK_LIBS_SYSTEMS_API})

install(TARGETS ${PROJECT_NAME}_server DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS ${PROJECT_NAME}_client DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
# install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
