cmake_minimum_required(VERSION 3.12)
project(celte-functional-tests)

include(FetchContent)


FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.11.0
)

enable_testing()

FetchContent_MakeAvailable(googletest)

find_package(msgpack CONFIG REQUIRED NAMES msgpack-cxx)
find_package(RdKafka CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(msgpack CONFIG REQUIRED NAMES msgpack-cxx)
find_package(glm CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread json)


# Add pkg-config flags for glib-2.0
link_directories(${GLIB_LIBRARY_DIRS})
add_definitions(${GLIB_CFLAGS_OTHER})

pkg_check_modules(GLIB REQUIRED glib-2.0)


# Set the source files for the functional tests
set(SOURCES
    test_server_single_node.cpp
)

set(SOURCES_RPC
    test_rpc.cpp
)

add_executable(test_server_single_node ${SOURCES})
target_compile_definitions(test_server_single_node PRIVATE -DCELTE_SERVER_MODE_ENABLED)
target_link_libraries(test_server_single_node gtest gtest_main)
target_link_libraries(test_server_single_node celtesystems_runtime_server msgpack-cxx RdKafka::rdkafka RdKafka::rdkafka++ ${GLIB_LIBRARIES} glm::glm Boost::thread Boost::json)

set(SOURCES_SINGLE_NODE_CLIENT
    test_client_single_node.cpp
)

add_executable(test_client_single_node ${SOURCES_SINGLE_NODE_CLIENT})
target_link_libraries(test_client_single_node gtest gtest_main)
target_link_libraries(test_client_single_node celtesystems_runtime_client msgpack-cxx RdKafka::rdkafka RdKafka::rdkafka++ ${GLIB_LIBRARIES} glm::glm)

add_executable(test_rpc ${SOURCES_RPC})
target_compile_definitions(test_rpc PRIVATE -DCELTE_SERVER_MODE_ENABLED)
target_link_libraries(test_rpc gtest gtest_main)
target_link_libraries(test_rpc celtesystems_runtime_server celtesystems_runtime_server msgpack-cxx RdKafka::rdkafka RdKafka::rdkafka++ ${GLIB_LIBRARIES} glm::glm)
