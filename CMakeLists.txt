cmake_minimum_required(VERSION 3.15)
project(StarEngine VERSION 0.3 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external/raylib)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")

set(MODULES_GENERATED_FILE "${CMAKE_BINARY_DIR}/Modules.modules.generated.h")

add_custom_command(
    OUTPUT ${MODULES_GENERATED_FILE}
    COMMAND ${CMAKE_COMMAND} -E echo "Generating Modules.modules.generated.h"
    COMMAND ${CMAKE_COMMAND} 
        -DMODULES_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src/Modules"
        -DMODULES_OUTPUT_FILE="${MODULES_GENERATED_FILE}"
        -P "${CMAKE_SOURCE_DIR}/cmake/GenerateModules.cmake"
    COMMENT "Generating module registration"
)

add_executable(StarEngine ${SOURCES} ${MODULES_GENERATED_FILE})

target_include_directories(StarEngine PRIVATE src)
target_include_directories(StarEngine PRIVATE "${CMAKE_SOURCE_DIR}/external/raygui")
target_include_directories(StarEngine PRIVATE "${CMAKE_BINARY_DIR}")

target_link_libraries(StarEngine PRIVATE raylib)

target_compile_definitions(StarEngine PRIVATE STARENGINE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/src")

if(WIN32)
    target_link_libraries(StarEngine PRIVATE comdlg32)
endif()