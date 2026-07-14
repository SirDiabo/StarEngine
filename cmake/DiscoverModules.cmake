set(STARENGINE_ENGINE_SRC_DIR "${CMAKE_SOURCE_DIR}/src")

set(STARENGINE_MODULE_ROOTS "${CMAKE_SOURCE_DIR}/src/Modules")

if(WIN32)
    set(_starengine_home "$ENV{USERPROFILE}/.starengine")
else()
    set(_starengine_home "$ENV{HOME}/.starengine")
endif()
set(_active_project_cfg "${_starengine_home}/active_project.cfg")

set(STARENGINE_PROJECT_ROOT "")
if(EXISTS "${_active_project_cfg}")
    file(STRINGS "${_active_project_cfg}" _active_project_lines)
    foreach(_line ${_active_project_lines})
        if(_line MATCHES "^ProjectRoot=(.*)$")
            set(STARENGINE_PROJECT_ROOT "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endif()

if(STARENGINE_PROJECT_ROOT AND EXISTS "${STARENGINE_PROJECT_ROOT}/Modules")
    list(APPEND STARENGINE_MODULE_ROOTS "${STARENGINE_PROJECT_ROOT}/Modules")
    set(STARENGINE_PROJECT_MODULES_DIR "${STARENGINE_PROJECT_ROOT}/Modules")
else()
    set(STARENGINE_PROJECT_MODULES_DIR "")
endif()

set(STARENGINE_MODULE_PLAIN_SOURCES "")

foreach(MODULE_ROOT ${STARENGINE_MODULE_ROOTS})
    string(REPLACE "\\" "/" _normalized_root "${MODULE_ROOT}")
    file(GLOB _starengine_module_dirs CONFIGURE_DEPENDS "${_normalized_root}/*")
    foreach(_module_dir ${_starengine_module_dirs})
        string(REPLACE "\\" "/" _normalized_dir "${_module_dir}")
        if(NOT IS_DIRECTORY "${_normalized_dir}")
            continue()
        endif()

        if(EXISTS "${_normalized_dir}/CMakeLists.txt")
            
            get_filename_component(_module_name "${_normalized_dir}" NAME)
            add_subdirectory("${_normalized_dir}" "${CMAKE_BINARY_DIR}/modules/${_module_name}")
        elseif(NOT "${_normalized_root}" STREQUAL "${CMAKE_SOURCE_DIR}/src/Modules")
            
            file(GLOB_RECURSE _module_sources CONFIGURE_DEPENDS
                "${_normalized_dir}/*.cpp" "${_normalized_dir}/*.c")
            list(APPEND STARENGINE_MODULE_PLAIN_SOURCES ${_module_sources})
        endif()
    endforeach()
endforeach()

get_property(STARENGINE_MODULE_LIBRARIES GLOBAL PROPERTY STARENGINE_MODULE_LIBRARIES)
