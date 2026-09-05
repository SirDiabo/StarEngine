cmake_minimum_required(VERSION 3.15)
project(StarEngine VERSION 0.7 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

option(STARENGINE_BUILD_GAME "Build the runtime-only packaged game instead of the editor" OFF)

add_subdirectory(external/raylib)

include(cmake/StarEngineModule.cmake)

# Discovers built-in + project modules
include(cmake/DiscoverModules.cmake)

set(MODULES_GENERATED_FILE "${CMAKE_BINARY_DIR}/Modules.modules.generated.h")

add_custom_target(generate_modules ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Generating Modules.modules.generated.h"
    COMMAND ${CMAKE_COMMAND}
        -DMODULES_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src/Modules"
        -DMODULES_PROJECT_DIR="${STARENGINE_PROJECT_MODULES_DIR}"
        -DMODULES_ENGINE_DIR="${STARENGINE_ENGINE_MODULES_DIR}"
        -DSTARENGINE_BUILD_GAME="${STARENGINE_BUILD_GAME}"
        -DMODULES_OUTPUT_FILE="${MODULES_GENERATED_FILE}"
        -P "${CMAKE_SOURCE_DIR}/cmake/GenerateModules.cmake"
    BYPRODUCTS ${MODULES_GENERATED_FILE}
    COMMENT "Generating module registration"
    VERBATIM
)

if(STARENGINE_BUILD_GAME)
    file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "src/Core/*.cpp" "src/Runtime/*.cpp" "src/Modules/*.cpp")
    list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/Core/Application\\.cpp$")
    list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/Modules/[^/]+/Editor/.*")
    list(APPEND GAME_SOURCES ${STARENGINE_MODULE_PLAIN_SOURCES})

    add_executable(StarEngineGame ${GAME_SOURCES} src/GameMain.cpp ${MODULES_GENERATED_FILE})
    add_dependencies(StarEngineGame generate_modules)

    target_include_directories(StarEngineGame PRIVATE src)
    target_include_directories(StarEngineGame PRIVATE "${CMAKE_BINARY_DIR}")

    target_link_libraries(StarEngineGame PRIVATE raylib)
    if(STARENGINE_MODULE_LIBRARIES)
        target_link_libraries(StarEngineGame PRIVATE ${STARENGINE_MODULE_LIBRARIES})
    endif()
    if(PLATFORM STREQUAL "RGFW" AND UNIX AND NOT APPLE)
        target_link_libraries(StarEngineGame PRIVATE Xrandr)
    endif()
    if(WIN32)
        set_target_properties(StarEngineGame PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MSVC)
            set_target_properties(StarEngineGame PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
        endif()
    endif()

    target_compile_definitions(StarEngineGame PRIVATE STARENGINE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/src")
    target_compile_definitions(StarEngineGame PRIVATE STARENGINE_GAME_BUILD=1)

    if(MINGW)
        target_link_options(StarEngineGame PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
else()
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
    list(FILTER SOURCES EXCLUDE REGEX ".*/GameMain\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/Tools/.*\\.cpp$")
    list(APPEND SOURCES ${STARENGINE_MODULE_PLAIN_SOURCES})
    list(APPEND SOURCES ${STARENGINE_MODULE_PLAIN_SOURCES_EDITOR_ONLY})

    add_executable(StarEngine ${SOURCES} ${MODULES_GENERATED_FILE})
    add_dependencies(StarEngine generate_modules)

    target_include_directories(StarEngine PRIVATE src)
    target_include_directories(StarEngine PRIVATE "${CMAKE_SOURCE_DIR}/external/raygui")
    target_include_directories(StarEngine PRIVATE "${CMAKE_BINARY_DIR}")

    target_link_libraries(StarEngine PRIVATE raylib)
    if(STARENGINE_MODULE_LIBRARIES)
        target_link_libraries(StarEngine PRIVATE ${STARENGINE_MODULE_LIBRARIES})
    endif()
    if(STARENGINE_MODULE_LIBRARIES_EDITOR_ONLY)
        target_link_libraries(StarEngine PRIVATE ${STARENGINE_MODULE_LIBRARIES_EDITOR_ONLY})
    endif()

    target_compile_definitions(StarEngine PRIVATE STARENGINE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/src")
    target_compile_definitions(StarEngine PRIVATE STARENGINE_BUILD_CONFIG="$<CONFIG>")
    target_compile_definitions(StarEngine PRIVATE STARENGINE_VERSION="${PROJECT_VERSION}")

    if(WIN32)
        target_link_libraries(StarEngine PRIVATE comdlg32 shell32 ole32)

        set_target_properties(StarEngine PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MSVC)
        
            set_target_properties(StarEngine PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
        endif()
        target_compile_definitions(StarEngine PRIVATE $<$<CONFIG:Debug>:STARENGINE_DEBUG_CONSOLE>)
    endif()
    
    add_executable(AssetPacker src/Tools/AssetPacker.cpp src/Core/StarPak.cpp)
    target_include_directories(AssetPacker PRIVATE src)

    add_custom_command(TARGET StarEngine POST_BUILD
        COMMAND "$<TARGET_FILE:AssetPacker>" "${CMAKE_SOURCE_DIR}/Assets" "$<TARGET_FILE:StarEngine>"
        COMMENT "Embedding Assets/ into StarEngine executable"
        VERBATIM
    )
    add_dependencies(StarEngine AssetPacker)
endif()