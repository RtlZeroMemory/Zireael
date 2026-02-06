cmake_minimum_required(VERSION 3.21)

project(Zireael LANGUAGES C)

option(ZIREAEL_BUILD_SHARED "Build shared library (optional)" OFF)
option(ZIREAEL_BUILD_EXAMPLES "Build examples" ON)
option(ZIREAEL_BUILD_TESTS "Build tests" ON)
option(ZIREAEL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (CI)" OFF)
option(ZIREAEL_SANITIZE_ADDRESS "Enable AddressSanitizer (Clang/GCC only)" OFF)
option(ZIREAEL_SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer (Clang/GCC only)" OFF)

add_library(zireael_build_settings INTERFACE)

set(ZIREAEL_MSVC_LIKE OFF)
if (MSVC)
    set(ZIREAEL_MSVC_LIKE ON)
elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    # clang-cl uses the MSVC frontend/diagnostic model.
    set(ZIREAEL_MSVC_LIKE ON)
endif ()

if (ZIREAEL_MSVC_LIKE)
    target_compile_options(zireael_build_settings INTERFACE /W4)
    target_compile_definitions(zireael_build_settings INTERFACE _CRT_SECURE_NO_WARNINGS)
else ()
    target_compile_options(zireael_build_settings INTERFACE -Wall -Wextra -Wpedantic)
endif ()

if (ZIREAEL_WARNINGS_AS_ERRORS)
    if (ZIREAEL_MSVC_LIKE)
        target_compile_options(zireael_build_settings INTERFACE /WX)
    else ()
        target_compile_options(zireael_build_settings INTERFACE -Werror)
    endif ()
endif ()

if (ZIREAEL_SANITIZE_ADDRESS OR ZIREAEL_SANITIZE_UNDEFINED)
    if (MSVC)
        message(FATAL_ERROR "ZIREAEL_SANITIZE_* is not supported with MSVC/clang-cl.")
    endif ()

    set(_zr_sanitize_list "")
    if (ZIREAEL_SANITIZE_ADDRESS)
        list(APPEND _zr_sanitize_list "address")
    endif ()
    if (ZIREAEL_SANITIZE_UNDEFINED)
        list(APPEND _zr_sanitize_list "undefined")
    endif ()
    string(JOIN "," _zr_sanitize_flags ${_zr_sanitize_list})

    target_compile_options(zireael_build_settings INTERFACE
            -fno-omit-frame-pointer
            -fno-sanitize-recover=all
            -fsanitize=${_zr_sanitize_flags}
    )
    target_link_options(zireael_build_settings INTERFACE
            -fsanitize=${_zr_sanitize_flags}
    )
endif ()

set(ZIREAEL_SOURCES
        src/core/zr_config.c
        src/core/zr_damage.c
        src/core/zr_diff.c
        src/core/zr_debug_overlay.c
        src/core/zr_debug_trace.c
        src/core/zr_drawlist.c
        src/core/zr_engine.c
        src/core/zr_event_pack.c
        src/core/zr_event_queue.c
        src/core/zr_framebuffer.c
        src/core/zr_input_parser.c
        src/core/zr_metrics.c
        src/platform/zr_platform_select.c
        src/unicode/zr_grapheme.c
        src/unicode/zr_unicode_data.c
        src/unicode/zr_utf8.c
        src/unicode/zr_width.c
        src/unicode/zr_wrap.c
        src/util/zr_arena.c
        src/util/zr_caps.c
        src/util/zr_log.c
        src/util/zr_ring.c
        src/util/zr_string_builder.c
        src/util/zr_vec.c
)

if (WIN32)
    list(APPEND ZIREAEL_SOURCES
            src/platform/win32/zr_plat_win32.c
    )
else ()
    list(APPEND ZIREAEL_SOURCES
            src/platform/posix/zr_plat_posix.c
    )
endif ()

add_library(zireael STATIC
        ${ZIREAEL_SOURCES}
)
add_library(Zireael::zireael ALIAS zireael)

set_target_properties(zireael PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
)

target_link_libraries(zireael PRIVATE zireael_build_settings)

target_include_directories(zireael
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

if (ZIREAEL_BUILD_SHARED)
    add_library(zireael_shared SHARED
            ${ZIREAEL_SOURCES}
    )
    add_library(Zireael::zireael_shared ALIAS zireael_shared)

    set_target_properties(zireael_shared PROPERTIES
            OUTPUT_NAME zireael
            C_STANDARD 11
            C_STANDARD_REQUIRED YES
            C_EXTENSIONS NO
    )
    if (WIN32)
        set_target_properties(zireael_shared PROPERTIES
                # On Windows, SHARED libraries also produce an import library (`.lib`).
                # Keep the DLL name stable (`zireael.dll`) but avoid colliding with the
                # static library output (`zireael.lib`).
                ARCHIVE_OUTPUT_NAME zireael_shared
                WINDOWS_EXPORT_ALL_SYMBOLS YES
        )
    endif ()

    target_link_libraries(zireael_shared PRIVATE zireael_build_settings)

    target_include_directories(zireael_shared
            PUBLIC
            ${PROJECT_SOURCE_DIR}/include
            PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )
endif ()

if (ZIREAEL_BUILD_EXAMPLES)
    add_executable(zr_example_minimal_render_loop
            examples/minimal_render_loop.c
    )
    target_link_libraries(zr_example_minimal_render_loop PRIVATE zireael_build_settings)
    target_link_libraries(zr_example_minimal_render_loop PRIVATE zireael)

    add_executable(zr_example_input_echo
            examples/input_echo.c
    )
    target_link_libraries(zr_example_input_echo PRIVATE zireael_build_settings)
    target_link_libraries(zr_example_input_echo PRIVATE zireael)

    add_executable(zr_example_resize_redraw
            examples/resize_redraw.c
    )
    target_link_libraries(zr_example_resize_redraw PRIVATE zireael_build_settings)
    target_link_libraries(zr_example_resize_redraw PRIVATE zireael)
endif ()

if (ZIREAEL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
