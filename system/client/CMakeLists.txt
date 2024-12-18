cmake_minimum_required(VERSION 3.27)
project(celtesystems)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(glm CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(include ${GLIB_INCLUDE_DIRS})

# Add pkg-config flags for glib-2.0
link_directories(${GLIB_LIBRARY_DIRS})
add_definitions(${GLIB_CFLAGS_OTHER})

add_library(CelteClient SHARED
    src/CelteClient.cpp
    src/StateConnected.cpp
    src/StateConnecting.cpp
    src/StateDisconnected.cpp
)

target_link_libraries(CelteClient PRIVATE ${GLIB_LIBRARIES}glm::glm)

install(TARGETS CelteClient
    LIBRARY DESTINATION celte-libraries
    PUBLIC_HEADER DESTINATION include
)