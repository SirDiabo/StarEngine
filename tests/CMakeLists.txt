add_custom_target(star_tests)
find_package(Threads REQUIRED)

set(STAR_SRC "${CMAKE_SOURCE_DIR}/src")

function(star_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})

    add_executable(${name} ${name}.cpp ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE "${STAR_SRC}" "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(${name} PRIVATE cxx_std_17)

    if(ARG_LIBRARIES)
        target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES})
    endif()

    if(MSVC)
        target_compile_definitions(${name} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${name} PRIVATE /utf-8)
    endif()

    if(STARENGINE_TEST_SANITIZE AND NOT MSVC)
        target_compile_options(${name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
        target_link_options(${name} PRIVATE -fsanitize=address,undefined)
    endif()

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 300)
    add_dependencies(star_tests ${name})
endfunction()

star_add_test(test_crypto
    SOURCES
        TestModuleStub.cpp
        "${STAR_SRC}/Core/PackageCipher.cpp")

star_add_test(test_baked_key
    SOURCES
        "${STAR_SRC}/Core/BakedKey.cpp")

star_add_test(test_cooked_text
    SOURCES
        "${STAR_SRC}/Core/CookedText.cpp")

star_add_test(test_cooked_model
    SOURCES
        "${STAR_SRC}/Core/CookedModel.cpp")

star_add_test(test_main_thread_queue
    SOURCES
        "${STAR_SRC}/Core/MainThreadQueue.cpp"
    LIBRARIES Threads::Threads)

star_add_test(test_wav_memory
    SOURCES
        "${STAR_SRC}/Runtime/Audio/WavFile.cpp"
    LIBRARIES Threads::Threads)

star_add_test(test_cooked_entry
    SOURCES
        TestModuleStub.cpp
        "${STAR_SRC}/Core/CookedEntry.cpp"
        "${STAR_SRC}/Core/StarPak.cpp"
        "${STAR_SRC}/Core/PackageCipher.cpp"
        "${STAR_SRC}/Core/PackageCompression.cpp"
        "${STAR_SRC}/Core/Profiler.cpp"
    LIBRARIES raylib)

star_add_test(test_package_cooker
    SOURCES
        TestModuleStub.cpp
        "${STAR_SRC}/Core/PackageCooker.cpp"
        "${STAR_SRC}/Core/ObjMtlParser.cpp"
        "${STAR_SRC}/Core/PathSafety.cpp"
        "${STAR_SRC}/Core/StarPak.cpp"
        "${STAR_SRC}/Core/PackageCipher.cpp"
        "${STAR_SRC}/Core/PackageCompression.cpp"
        "${STAR_SRC}/Core/CookedText.cpp"
        "${STAR_SRC}/Core/CookedEntry.cpp"
        "${STAR_SRC}/Core/Profiler.cpp"
    LIBRARIES raylib)

star_add_test(test_cooked_file
    SOURCES
        TestModuleStub.cpp
        "${STAR_SRC}/Core/CookedFile.cpp"
        "${STAR_SRC}/Core/CookedEntry.cpp"
        "${STAR_SRC}/Core/ContentPathResolver.cpp"
        "${STAR_SRC}/Core/CookedText.cpp"
        "${STAR_SRC}/Core/StarPak.cpp"
        "${STAR_SRC}/Core/PackageCipher.cpp"
        "${STAR_SRC}/Core/PackageCompression.cpp"
        "${STAR_SRC}/Core/Profiler.cpp"
    LIBRARIES raylib)

star_add_test(test_inflate
    SOURCES
        TestModuleStub.cpp
        "${STAR_SRC}/Core/PackageCompression.cpp"
    LIBRARIES raylib)

star_add_test(test_starpak
    SOURCES
        "${STAR_SRC}/Core/StarPak.cpp")

star_add_test(test_paths_process
    SOURCES
        "${STAR_SRC}/Core/PathSafety.cpp"
        "${STAR_SRC}/Core/SafeProcess.cpp")

star_add_test(test_trust_cache
    SOURCES
        "${STAR_SRC}/Core/TrustStore.cpp"
        "${STAR_SRC}/Core/BuildCacheGuard.cpp"
        "${STAR_SRC}/Core/PathSafety.cpp")

star_add_test(test_culling
    LIBRARIES raylib)

star_add_test(test_engine_root
    SOURCES
        "${STAR_SRC}/Core/EngineRoot.cpp"
        "${STAR_SRC}/Core/ExecutablePath.cpp")

star_add_test(test_panel_cache)
