cmake_minimum_required(VERSION 3.16)
project(GroupChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # lets VS Code pick up include paths

# Threads
find_package(Threads REQUIRED)

# Header-only "protocol" target that propagates include/
add_library(protocol INTERFACE)
target_include_directories(protocol INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Server
add_executable(server src/server.cpp)
target_link_libraries(server PRIVATE Threads::Threads protocol)

# Client
add_executable(client src/client.cpp)
target_link_libraries(client PRIVATE Threads::Threads protocol)

# Optional: nicer warnings (GNU/Clang)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic)
endif()
