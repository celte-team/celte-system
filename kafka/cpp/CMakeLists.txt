cmake_minimum_required(VERSION 3.27)
project(celtesystems)

set(CMAKE_CXX_STANDARD 20)

find_package(RdKafka CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(include ${GLIB_INCLUDE_DIRS} ${RDKAFKA_INCLUDE_DIRS})

# Add pkg-config flags for glib-2.0
link_directories(${GLIB_LIBRARY_DIRS})
add_definitions(${GLIB_CFLAGS_OTHER})

set(KLSOURCES
    src/states/KLDisconnected.cpp
    src/states/KLErrorCouldNotConnect.cpp
    src/states/KLConnected.cpp
    src/KafkaFSM.cpp
)
add_library(KafkaLink ${KLSOURCES})
target_link_libraries(KafkaLink PRIVATE RdKafka::rdkafka RdKafka::rdkafka++ ${GLIB_LIBRARIES})

install(TARGETS KafkaLink
    LIBRARY DESTINATION celte-libraries
    PUBLIC_HEADER DESTINATION include
)