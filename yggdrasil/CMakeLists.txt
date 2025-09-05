cmake_minimum_required(VERSION 3.10)
project(yggdrasil LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${ENV}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

find_package(glm CONFIG REQUIRED)
find_package(unofficial-pulsar CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCES
    src/main.cpp
    src/Yggdrasil.cpp
    src/KMeans.cpp
)

# Generated protobuf sources from ../system/protogen
set(PROTO_SRCS
    ${CMAKE_SOURCE_DIR}/../system/protogen/systems_structs.pb.cc
)

add_executable(yggdrasil_example ${SOURCES} ${PROTO_SRCS})
target_include_directories(yggdrasil_example PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(yggdrasil_example PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/../system/protogen)
target_link_libraries(yggdrasil_example PRIVATE glm::glm unofficial::pulsar::pulsar hiredis::hiredis cpr::cpr protobuf::libprotobuf)

install(TARGETS yggdrasil_example DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

enable_testing()

set(TEST_SOURCES
    tests/test_kmeans.cpp
)

add_executable(yggdrasil_tests ${TEST_SOURCES} src/KMeans.cpp ${PROTO_SRCS})
target_include_directories(yggdrasil_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(yggdrasil_tests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/../system/protogen)
target_link_libraries(yggdrasil_tests PRIVATE GTest::gtest_main glm::glm protobuf::libprotobuf)

add_test(NAME KMeansTest COMMAND yggdrasil_tests)
