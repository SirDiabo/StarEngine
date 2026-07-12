cmake_minimum_required(VERSION 3.15)
project(StarEngine VERSION 0.4 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

option(STARENGINE_BUILD_GAME "Build the runtime-only packaged game instead of the editor" OFF)

add_subdirectory(external/raylib)

set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
set(NO_MEDIA ON CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
set(USE_NICE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/libdatachannel)

set(MODULES_GENERATED_FILE "${CMAKE_BINARY_DIR}/Modules.modules.generated.h")

add_custom_target(generate_modules ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Generating Modules.modules.generated.h"
    COMMAND ${CMAKE_COMMAND}
        -DMODULES_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src/Modules"
        -DMODULES_OUTPUT_FILE="${MODULES_GENERATED_FILE}"
        -P "${CMAKE_SOURCE_DIR}/cmake/GenerateModules.cmake"
    BYPRODUCTS ${MODULES_GENERATED_FILE}
    COMMENT "Generating module registration"
)

if(STARENGINE_BUILD_GAME)
    file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "src/Core/*.cpp" "src/Runtime/*.cpp" "src/Modules/*.cpp")
    list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/Core/Application\\.cpp$")

    add_executable(StarEngineGame ${GAME_SOURCES} src/GameMain.cpp ${MODULES_GENERATED_FILE})
    add_dependencies(StarEngineGame generate_modules)

    target_include_directories(StarEngineGame PRIVATE src)
    target_include_directories(StarEngineGame PRIVATE "${CMAKE_BINARY_DIR}")

    target_link_libraries(StarEngineGame PRIVATE raylib)
    target_link_libraries(StarEngineGame PRIVATE LibDataChannel::LibDataChannel)
    if(PLATFORM STREQUAL "RGFW" AND UNIX AND NOT APPLE)
        target_link_libraries(StarEngineGame PRIVATE Xrandr)
    endif()
    if(WIN32)
        target_link_libraries(StarEngineGame PRIVATE ws2_32)
    endif()

    target_compile_definitions(StarEngineGame PRIVATE STARENGINE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/src")
    target_compile_definitions(StarEngineGame PRIVATE STARENGINE_GAME_BUILD=1)

    if(MINGW)
        target_link_options(StarEngineGame PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
else()
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
    list(FILTER SOURCES EXCLUDE REGEX ".*/GameMain\\.cpp$")

    add_executable(StarEngine ${SOURCES} ${MODULES_GENERATED_FILE})
    add_dependencies(StarEngine generate_modules)

    target_include_directories(StarEngine PRIVATE src)
    target_include_directories(StarEngine PRIVATE "${CMAKE_SOURCE_DIR}/external/raygui")
    target_include_directories(StarEngine PRIVATE "${CMAKE_BINARY_DIR}")

    target_link_libraries(StarEngine PRIVATE raylib)
    target_link_libraries(StarEngine PRIVATE LibDataChannel::LibDataChannel)

    target_compile_definitions(StarEngine PRIVATE STARENGINE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/src")

    if(WIN32)
        target_link_libraries(StarEngine PRIVATE comdlg32 shell32 ole32 ws2_32)
    endif()
endif()