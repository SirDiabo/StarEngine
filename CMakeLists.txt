cmake_minimum_required(VERSION 3.15)
project(StarEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external/raylib)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(StarEngine ${SOURCES})

target_include_directories(StarEngine PRIVATE src)
target_link_libraries(StarEngine PRIVATE raylib)