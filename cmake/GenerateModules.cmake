cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0007 NEW)

if(NOT DEFINED MODULES_SOURCE_DIR)
    message(FATAL_ERROR "MODULES_SOURCE_DIR not set")
endif()

if(NOT DEFINED MODULES_OUTPUT_FILE)
    message(FATAL_ERROR "MODULES_OUTPUT_FILE not set")
endif()

function(_starengine_unquote var)
    set(_value "${${var}}")
    string(LENGTH "${_value}" _len)
    if(_len GREATER 1 AND "${_value}" MATCHES "^\"(.*)\"$")
        set(_value "${CMAKE_MATCH_1}")
    endif()
    set(${var} "${_value}" PARENT_SCOPE)
endfunction()

_starengine_unquote(MODULES_SOURCE_DIR)
_starengine_unquote(MODULES_OUTPUT_FILE)
_starengine_unquote(MODULES_PROJECT_DIR)
_starengine_unquote(MODULES_ENGINE_DIR)

if(NOT DEFINED STARENGINE_BUILD_GAME)
    set(STARENGINE_BUILD_GAME OFF)
endif()

string(REPLACE "\\ " " " _normalized_output "${MODULES_OUTPUT_FILE}")

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/src/Modules)

set(MODULE_NAMES "")
set(MODULE_HEADERS "")
set(_starengine_gen_seen_names "")

function(starengine_gen_add_module MODULE_NAME MODULE_HEADER MANIFEST_FILE)
    list(FIND _starengine_gen_seen_names "${MODULE_NAME}" _seen_index)
    if(NOT _seen_index EQUAL -1)
        return()
    endif()

    set(_editor_only FALSE)
    if(EXISTS "${MANIFEST_FILE}")
        file(STRINGS "${MANIFEST_FILE}" _manifest_lines)
        foreach(_manifest_line ${_manifest_lines})
            if(_manifest_line STREQUAL "editorOnly=true")
                set(_editor_only TRUE)
            endif()
        endforeach()
    endif()

    set(_starengine_gen_seen_names "${_starengine_gen_seen_names};${MODULE_NAME}" PARENT_SCOPE)

    if(_editor_only AND STARENGINE_BUILD_GAME)
        return()
    endif()

    set(MODULE_NAMES "${MODULE_NAMES};${MODULE_NAME}Module" PARENT_SCOPE)
    set(MODULE_HEADERS "${MODULE_HEADERS};${MODULE_HEADER}" PARENT_SCOPE)
endfunction()

string(REPLACE "\\" "/" _normalized_source "${MODULES_SOURCE_DIR}")
file(GLOB MODULE_DIRS "${_normalized_source}/*")
foreach(MODULE_DIR ${MODULE_DIRS})
    string(REPLACE "\\" "/" _normalized_dir "${MODULE_DIR}")
    if(IS_DIRECTORY ${_normalized_dir})
        get_filename_component(MODULE_NAME ${_normalized_dir} NAME)
        if(EXISTS "${_normalized_dir}/${MODULE_NAME}.starmodule")
            starengine_gen_add_module("${MODULE_NAME}" "Modules/${MODULE_NAME}/${MODULE_NAME}Module.h"
                "${_normalized_dir}/${MODULE_NAME}.starmodule")
        endif()
    endif()
endforeach()

set(_starengine_external_module_roots "")
if(DEFINED MODULES_PROJECT_DIR AND EXISTS "${MODULES_PROJECT_DIR}")
    list(APPEND _starengine_external_module_roots "${MODULES_PROJECT_DIR}")
endif()
if(DEFINED MODULES_ENGINE_DIR AND EXISTS "${MODULES_ENGINE_DIR}")
    list(APPEND _starengine_external_module_roots "${MODULES_ENGINE_DIR}")
endif()

foreach(_external_root ${_starengine_external_module_roots})
    string(REPLACE "\\" "/" _normalized_ext "${_external_root}")
    file(GLOB _ext_module_dirs "${_normalized_ext}/*")
    foreach(_ext_module_dir ${_ext_module_dirs})
        string(REPLACE "\\" "/" _normalized_dir "${_ext_module_dir}")
        if(IS_DIRECTORY ${_normalized_dir})
            file(GLOB _manifest_files "${_normalized_dir}/*.starmodule")
            list(LENGTH _manifest_files _manifest_count)
            if(_manifest_count GREATER 0)
                list(GET _manifest_files 0 _manifest_file)
                file(STRINGS "${_manifest_file}" _manifest_name_line REGEX "^name=")
                string(REPLACE "name=" "" _module_name "${_manifest_name_line}")
                string(STRIP "${_module_name}" _module_name)
                if(_module_name)
                    starengine_gen_add_module("${_module_name}" "${_module_name}Module.h" "${_manifest_file}")
                endif()
            endif()
        endif()
    endforeach()
endforeach()

file(WRITE ${_normalized_output}
"#pragma once
#include \"Core/Module.h\"

")

foreach(HEADER ${MODULE_HEADERS})
    file(APPEND ${_normalized_output} "#include \"${HEADER}\"\n")
endforeach()

file(APPEND ${_normalized_output} "\ninline void RegisterAllModules() {\n")

foreach(NAME ${MODULE_NAMES})
    file(APPEND ${_normalized_output} "    ModuleManager::Get().Register(new ${NAME}());\n")
endforeach()

file(APPEND ${_normalized_output} "}\n")

message(STATUS "Generated ${_normalized_output} with ${MODULE_NAMES}")
