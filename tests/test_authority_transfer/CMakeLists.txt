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
    auth_c2c_server.cpp
)



add_executable(auth_c2c_server ${SOURCES})
target_compile_definitions(auth_c2c_server PRIVATE -DCELTE_SERVER_MODE_ENABLED)
target_link_libraries(auth_c2c_server gtest gtest_main)
target_link_libraries(auth_c2c_server celtesystems_runtime_server msgpack-cxx  ${GLIB_LIBRARIES} glm::glm Boost::thread Boost::json)

set(SOURCES_CLIENT
    auth_c2c_client.cpp
)

add_executable(auth_c2c_client ${SOURCES_CLIENT})
target_link_libraries(auth_c2c_client gtest gtest_main)
target_link_libraries(auth_c2c_client celtesystems_runtime_client msgpack-cxx  ${GLIB_LIBRARIES} glm::glm)
