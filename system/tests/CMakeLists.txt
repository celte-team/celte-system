cmake_minimum_required(VERSION 3.12)
project(celte-unit-tests)

find_package(unofficial-pulsar CONFIG) # moving this after gtest breaks the build, idk but hey it works here :)

include(FetchContent)

# Fetch GoogleTest and GoogleMock
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.11.0
)

FetchContent_MakeAvailable(googletest)

enable_testing()

find_package(PkgConfig REQUIRED)
find_package(msgpack CONFIG REQUIRED NAMES msgpack-cxx)

pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(include
    ${GLIB_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/runtime/include
    ${CMAKE_SOURCE_DIR}/runtime/net/include
)

# Set the source files for the unit tests
set(SOURCES
    test_replicator.cpp
)

add_executable(test_server ${SOURCES})
target_compile_definitions(test_server PRIVATE -DCELTE_SERVER_MODE_ENABLED)
target_link_libraries(test_server gtest gmock gtest_main msgpack-cxx)
target_link_libraries(test_server celtesystems_runtime_server)

add_executable(test_celtenet ${CMAKE_SOURCE_DIR}/runtime/net/tests/test_celtenet.cpp)
target_link_libraries(test_celtenet gtest gmock gtest_main celtenet unofficial::pulsar::pulsar)

#  setup ctest
add_test(NAME test_celtenet COMMAND test_celtenet)

add_executable(client.debug debug/client.cpp)
target_link_libraries(client.debug gtest gmock gtest_main msgpack-cxx)
target_link_libraries(client.debug celtesystems_runtime_client celtenet)

add_executable(server.debug debug/server.cpp)
target_compile_definitions(server.debug PRIVATE -DCELTE_SERVER_MODE_ENABLED)
target_link_libraries(server.debug gtest gmock gtest_main msgpack-cxx)
target_link_libraries(server.debug celtesystems_runtime_server celtenet)