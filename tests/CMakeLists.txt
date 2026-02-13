add_library(zireael_unit_lib STATIC
        ${PROJECT_SOURCE_DIR}/src/core/zr_config.c
        ${PROJECT_SOURCE_DIR}/src/core/zr_damage.c
        ${PROJECT_SOURCE_DIR}/src/core/zr_diff.c
        ${PROJECT_SOURCE_DIR}/src/core/zr_debug_overlay.c
        ${PROJECT_SOURCE_DIR}/src/core/zr_debug_trace.c
        ${PROJECT_SOURCE_DIR}/src/core/zr_drawlist.c
        ${PROJECT_SOURCE_DIR}/src/core/zr_engine.c
        ${PROJECT_SOURCE_DIR}/src/core/zr_event_pack.c
        ${PROJECT_SOURCE_DIR}/src/core/zr_event_queue.c
        ${PROJECT_SOURCE_DIR}/src/core/zr_framebuffer.c
        ${PROJECT_SOURCE_DIR}/src/core/zr_input_parser.c
        ${PROJECT_SOURCE_DIR}/src/core/zr_metrics.c
        ${PROJECT_SOURCE_DIR}/src/unicode/zr_grapheme.c
        ${PROJECT_SOURCE_DIR}/src/unicode/zr_unicode_data.c
        ${PROJECT_SOURCE_DIR}/src/unicode/zr_utf8.c
        ${PROJECT_SOURCE_DIR}/src/unicode/zr_width.c
        ${PROJECT_SOURCE_DIR}/src/unicode/zr_wrap.c
        ${PROJECT_SOURCE_DIR}/src/util/zr_arena.c
        ${PROJECT_SOURCE_DIR}/src/util/zr_assert.c
        ${PROJECT_SOURCE_DIR}/src/util/zr_caps.c
        ${PROJECT_SOURCE_DIR}/src/util/zr_log.c
        ${PROJECT_SOURCE_DIR}/src/util/zr_ring.c
        ${PROJECT_SOURCE_DIR}/src/util/zr_string_builder.c
        ${PROJECT_SOURCE_DIR}/src/util/zr_vec.c
        unit/mock_platform.c
)

target_link_libraries(zireael_unit_lib PRIVATE zireael_build_settings)
target_compile_definitions(zireael_unit_lib PRIVATE
        ZR_ENGINE_TESTING=1
)
target_include_directories(zireael_unit_lib PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/tests
)

add_executable(zireael_unit_tests
        zr_test.c
        zr_tests_main.c
        unit/unit_smoke.c
        unit/test_arena.c
        unit/test_bytes.c
        unit/test_checked.c
        unit/test_damage_rects.c
        unit/test_debug_trace.c
        unit/test_diff_color_quant.c
        unit/test_diff_hotpath_telemetry.c
        unit/test_diff_spans.c
        unit/test_diff_term_state_validity.c
        unit/test_drawlist_execute.c
        unit/test_engine_poll_events_truncation.c
        unit/test_engine_tick_events.c
        unit/test_engine_input_sequences.c
        unit/test_engine_get_caps.c
        unit/test_engine_metrics_damage.c
        unit/test_engine_present_backpressure.c
        unit/test_engine_restore_hooks.c
        unit/test_engine_present_single_flush.c
        unit/test_engine_submit_drawlist_no_partial_effects.c
        unit/test_engine_version_negotiation.c
        unit/test_framebuffer_init_resize.c
        unit/test_cell_invariants.c
        unit/test_clipping.c
        unit/test_blit.c
        unit/test_framebuffer_text.c
        unit/test_framebuffer_primitives.c
        unit/test_drawlist_validate.c
        unit/test_event_pack.c
        unit/test_event_queue.c
        unit/test_limits.c
        unit/test_metrics_struct.c
        unit/test_overlay_render.c
        unit/test_public_abi_headers.c
        unit/test_ring.c
        unit/test_string_builder.c
        unit/test_user_event_injection.c
        unit/test_vec.c
        unit/test_utf8_decode.c
        unit/test_grapheme_iter.c
        unit/test_width.c
        unit/test_wrap.c
        golden/zr_golden.c
        golden/golden_diff_renderer.c
        golden/golden_runner_smoke.c
)

target_include_directories(zireael_unit_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/tests
)

target_compile_definitions(zireael_unit_tests PRIVATE
        ZR_TESTS_DIR="${PROJECT_SOURCE_DIR}/tests"
)

target_link_libraries(zireael_unit_tests PRIVATE zireael_build_settings)
target_link_libraries(zireael_unit_tests PRIVATE zireael_unit_lib)

add_executable(zireael_perf_diff_hotpath
        perf/diff_hotpath_bench.c
)
target_include_directories(zireael_perf_diff_hotpath PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/tests
)
target_link_libraries(zireael_perf_diff_hotpath PRIVATE zireael_build_settings)
target_link_libraries(zireael_perf_diff_hotpath PRIVATE zireael_unit_lib)

add_test(NAME zireael.unit COMMAND zireael_unit_tests --prefix unit.)
add_test(NAME zireael.golden COMMAND zireael_unit_tests --prefix golden.)

if (WIN32)
    add_executable(zireael_integration_win32_conpty_rawmode
            integration/win32_conpty_rawmode.c
            ${PROJECT_SOURCE_DIR}/src/platform/win32/zr_win32_conpty_test.c
    )
    target_include_directories(zireael_integration_win32_conpty_rawmode PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(zireael_integration_win32_conpty_rawmode PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_win32_conpty_rawmode PRIVATE zireael)
    add_test(NAME zireael.integration.win32.conpty.rawmode COMMAND zireael_integration_win32_conpty_rawmode)
    set_tests_properties(zireael.integration.win32.conpty.rawmode PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)

    add_executable(zireael_integration_win32_conpty_wake
            integration/win32_conpty_wake.c
            ${PROJECT_SOURCE_DIR}/src/platform/win32/zr_win32_conpty_test.c
    )
    target_include_directories(zireael_integration_win32_conpty_wake PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(zireael_integration_win32_conpty_wake PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_win32_conpty_wake PRIVATE zireael)
    add_test(NAME zireael.integration.win32.conpty.wake COMMAND zireael_integration_win32_conpty_wake)
    set_tests_properties(zireael.integration.win32.conpty.wake PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)

    add_executable(zireael_integration_win32_conpty_engine_post_user_event
            integration/win32_conpty_engine_post_user_event.c
            ${PROJECT_SOURCE_DIR}/src/platform/win32/zr_win32_conpty_test.c
    )
    target_include_directories(zireael_integration_win32_conpty_engine_post_user_event PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(zireael_integration_win32_conpty_engine_post_user_event PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_win32_conpty_engine_post_user_event PRIVATE zireael)
    add_test(NAME zireael.integration.win32.conpty.engine.post_user_event
            COMMAND zireael_integration_win32_conpty_engine_post_user_event)
    set_tests_properties(zireael.integration.win32.conpty.engine.post_user_event PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)

    add_executable(zireael_integration_win32_caps_overrides
            integration/win32_caps_overrides.c
            ${PROJECT_SOURCE_DIR}/src/platform/win32/zr_win32_conpty_test.c
    )
    target_include_directories(zireael_integration_win32_caps_overrides PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(zireael_integration_win32_caps_overrides PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_win32_caps_overrides PRIVATE zireael)
    add_test(NAME zireael.integration.win32.caps.overrides COMMAND zireael_integration_win32_caps_overrides)
    set_tests_properties(zireael.integration.win32.caps.overrides PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)
else ()
    find_package(Threads REQUIRED)

    add_executable(zireael_integration_posix_pty_rawmode
            integration/posix_pty_rawmode.c
    )
    target_include_directories(zireael_integration_posix_pty_rawmode PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(zireael_integration_posix_pty_rawmode PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_posix_pty_rawmode PRIVATE zireael)
    add_test(NAME zireael.integration.posix.pty.rawmode COMMAND zireael_integration_posix_pty_rawmode)
    set_tests_properties(zireael.integration.posix.pty.rawmode PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)

    add_executable(zireael_integration_posix_pty_wake
            integration/posix_pty_wake.c
    )
    target_include_directories(zireael_integration_posix_pty_wake PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(zireael_integration_posix_pty_wake PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_posix_pty_wake PRIVATE zireael)
    target_link_libraries(zireael_integration_posix_pty_wake PRIVATE Threads::Threads)
    add_test(NAME zireael.integration.posix.pty.wake COMMAND zireael_integration_posix_pty_wake)
    set_tests_properties(zireael.integration.posix.pty.wake PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)

    add_executable(zireael_integration_posix_engine_post_user_event
            integration/posix_engine_post_user_event.c
    )
    target_include_directories(zireael_integration_posix_engine_post_user_event PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(zireael_integration_posix_engine_post_user_event PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_posix_engine_post_user_event PRIVATE zireael)
    target_link_libraries(zireael_integration_posix_engine_post_user_event PRIVATE Threads::Threads)
    add_test(NAME zireael.integration.posix.engine.post_user_event COMMAND zireael_integration_posix_engine_post_user_event)
    set_tests_properties(zireael.integration.posix.engine.post_user_event PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)

    add_executable(zireael_integration_posix_create_fd_leak
            integration/posix_create_fd_leak.c
    )
    target_include_directories(zireael_integration_posix_create_fd_leak PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(zireael_integration_posix_create_fd_leak PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_posix_create_fd_leak PRIVATE zireael)
    add_test(NAME zireael.integration.posix.create.fd_leak COMMAND zireael_integration_posix_create_fd_leak)
    set_tests_properties(zireael.integration.posix.create.fd_leak PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)

    add_executable(zireael_integration_posix_caps_overrides
            integration/posix_caps_overrides.c
    )
    target_include_directories(zireael_integration_posix_caps_overrides PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(zireael_integration_posix_caps_overrides PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_posix_caps_overrides PRIVATE zireael)
    add_test(NAME zireael.integration.posix.caps.overrides COMMAND zireael_integration_posix_caps_overrides)
    set_tests_properties(zireael.integration.posix.caps.overrides PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)
endif ()

add_executable(zireael_fuzz_smoke
        fuzz/zr_fuzz_smoke.c
)
target_include_directories(zireael_fuzz_smoke PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(zireael_fuzz_smoke PRIVATE zireael_build_settings)
target_link_libraries(zireael_fuzz_smoke PRIVATE zireael)
add_test(NAME zireael.fuzz.smoke COMMAND zireael_fuzz_smoke)
set_tests_properties(zireael.fuzz.smoke PROPERTIES TIMEOUT 5)

add_executable(zireael_fuzz_drawlist_parser
        fuzz/fuzz_drawlist_parser.c
)
target_include_directories(zireael_fuzz_drawlist_parser PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(zireael_fuzz_drawlist_parser PRIVATE zireael_build_settings)
target_link_libraries(zireael_fuzz_drawlist_parser PRIVATE zireael)
add_test(NAME zireael.fuzz.drawlist.parser COMMAND zireael_fuzz_drawlist_parser)
set_tests_properties(zireael.fuzz.drawlist.parser PROPERTIES TIMEOUT 5)

add_executable(zireael_fuzz_input_parser
        fuzz/fuzz_input_parser.c
)
target_include_directories(zireael_fuzz_input_parser PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(zireael_fuzz_input_parser PRIVATE zireael_build_settings)
target_link_libraries(zireael_fuzz_input_parser PRIVATE zireael)
add_test(NAME zireael.fuzz.input.parser COMMAND zireael_fuzz_input_parser)
set_tests_properties(zireael.fuzz.input.parser PROPERTIES TIMEOUT 5)

add_executable(zireael_fuzz_utf8_decode
        fuzz/fuzz_utf8_decode.c
)
target_include_directories(zireael_fuzz_utf8_decode PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(zireael_fuzz_utf8_decode PRIVATE zireael_build_settings)
target_link_libraries(zireael_fuzz_utf8_decode PRIVATE zireael)
add_test(NAME zireael.fuzz.utf8.decode COMMAND zireael_fuzz_utf8_decode)
set_tests_properties(zireael.fuzz.utf8.decode PROPERTIES TIMEOUT 5)

add_executable(zireael_fuzz_grapheme_iter
        fuzz/fuzz_grapheme_iter.c
)
target_include_directories(zireael_fuzz_grapheme_iter PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(zireael_fuzz_grapheme_iter PRIVATE zireael_build_settings)
target_link_libraries(zireael_fuzz_grapheme_iter PRIVATE zireael)
add_test(NAME zireael.fuzz.grapheme.iter COMMAND zireael_fuzz_grapheme_iter)
set_tests_properties(zireael.fuzz.grapheme.iter PROPERTIES TIMEOUT 5)

if (ZIREAEL_BUILD_LIBFUZZER)
    if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "ZIREAEL_BUILD_LIBFUZZER requires Clang.")
    endif ()

    set(_zr_libfuzz_flags
            -fno-omit-frame-pointer
            -fno-sanitize-recover=all
            -fsanitize=fuzzer,address,undefined
    )

    function(zr_add_libfuzzer_target target_name source_file)
        add_executable(${target_name}
                ${source_file}
        )
        target_include_directories(${target_name} PRIVATE
                ${PROJECT_SOURCE_DIR}/src
        )
        target_link_libraries(${target_name} PRIVATE zireael_build_settings)
        target_link_libraries(${target_name} PRIVATE zireael)
        target_compile_options(${target_name} PRIVATE ${_zr_libfuzz_flags})
        target_link_options(${target_name} PRIVATE ${_zr_libfuzz_flags})
    endfunction()

    zr_add_libfuzzer_target(zireael_libfuzz_drawlist_parser fuzz/libfuzzer_drawlist_parser.c)
    zr_add_libfuzzer_target(zireael_libfuzz_input_parser fuzz/libfuzzer_input_parser.c)
    zr_add_libfuzzer_target(zireael_libfuzz_utf8_decode fuzz/libfuzzer_utf8_decode.c)
    zr_add_libfuzzer_target(zireael_libfuzz_grapheme_iter fuzz/libfuzzer_grapheme_iter.c)
endif ()
