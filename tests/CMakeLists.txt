add_executable(zireael_unit_tests
        zr_test.c
        zr_tests_main.c
        unit/unit_smoke.c
        unit/test_arena.c
        unit/test_bytes.c
        unit/test_checked.c
        unit/test_diff_color_quant.c
        unit/test_diff_spans.c
        unit/test_drawlist_execute.c
        unit/test_framebuffer_init_resize.c
        unit/test_cell_invariants.c
        unit/test_clipping.c
        unit/test_blit.c
        unit/test_framebuffer_text.c
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
        ${PROJECT_SOURCE_DIR}/tests
)

target_compile_definitions(zireael_unit_tests PRIVATE
        ZR_TESTS_DIR="${PROJECT_SOURCE_DIR}/tests"
)

target_link_libraries(zireael_unit_tests PRIVATE zireael_build_settings)
target_link_libraries(zireael_unit_tests PRIVATE zireael)

add_test(NAME zireael.unit COMMAND zireael_unit_tests --prefix unit.)
add_test(NAME zireael.golden COMMAND zireael_unit_tests --prefix golden.)

if (WIN32)
    add_executable(zireael_integration_win32_conpty_rawmode
            integration/win32_conpty_rawmode.c
            ${PROJECT_SOURCE_DIR}/src/platform/win32/zr_win32_conpty_test.c
    )
    target_link_libraries(zireael_integration_win32_conpty_rawmode PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_win32_conpty_rawmode PRIVATE zireael)
    add_test(NAME zireael.integration.win32.conpty.rawmode COMMAND zireael_integration_win32_conpty_rawmode)
    set_tests_properties(zireael.integration.win32.conpty.rawmode PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)

    add_executable(zireael_integration_win32_conpty_wake
            integration/win32_conpty_wake.c
            ${PROJECT_SOURCE_DIR}/src/platform/win32/zr_win32_conpty_test.c
    )
    target_link_libraries(zireael_integration_win32_conpty_wake PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_win32_conpty_wake PRIVATE zireael)
    add_test(NAME zireael.integration.win32.conpty.wake COMMAND zireael_integration_win32_conpty_wake)
    set_tests_properties(zireael.integration.win32.conpty.wake PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)
else ()
    find_package(Threads REQUIRED)

    add_executable(zireael_integration_posix_pty_rawmode
            integration/posix_pty_rawmode.c
    )
    target_link_libraries(zireael_integration_posix_pty_rawmode PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_posix_pty_rawmode PRIVATE zireael)
    add_test(NAME zireael.integration.posix.pty.rawmode COMMAND zireael_integration_posix_pty_rawmode)
    set_tests_properties(zireael.integration.posix.pty.rawmode PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)

    add_executable(zireael_integration_posix_pty_wake
            integration/posix_pty_wake.c
    )
    target_link_libraries(zireael_integration_posix_pty_wake PRIVATE zireael_build_settings)
    target_link_libraries(zireael_integration_posix_pty_wake PRIVATE zireael)
    target_link_libraries(zireael_integration_posix_pty_wake PRIVATE Threads::Threads)
    add_test(NAME zireael.integration.posix.pty.wake COMMAND zireael_integration_posix_pty_wake)
    set_tests_properties(zireael.integration.posix.pty.wake PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 10)
endif ()

add_executable(zireael_fuzz_smoke
        fuzz/zr_fuzz_smoke.c
)
target_link_libraries(zireael_fuzz_smoke PRIVATE zireael_build_settings)
target_link_libraries(zireael_fuzz_smoke PRIVATE zireael)
add_test(NAME zireael.fuzz.smoke COMMAND zireael_fuzz_smoke)
set_tests_properties(zireael.fuzz.smoke PROPERTIES TIMEOUT 5)

add_executable(zireael_fuzz_drawlist_parser
        fuzz/fuzz_drawlist_parser.c
)
target_link_libraries(zireael_fuzz_drawlist_parser PRIVATE zireael_build_settings)
target_link_libraries(zireael_fuzz_drawlist_parser PRIVATE zireael)
add_test(NAME zireael.fuzz.drawlist.parser COMMAND zireael_fuzz_drawlist_parser)
set_tests_properties(zireael.fuzz.drawlist.parser PROPERTIES TIMEOUT 5)

add_executable(zireael_fuzz_input_parser
        fuzz/fuzz_input_parser.c
)
target_link_libraries(zireael_fuzz_input_parser PRIVATE zireael_build_settings)
target_link_libraries(zireael_fuzz_input_parser PRIVATE zireael)
add_test(NAME zireael.fuzz.input.parser COMMAND zireael_fuzz_input_parser)
set_tests_properties(zireael.fuzz.input.parser PROPERTIES TIMEOUT 5)

add_executable(zireael_fuzz_utf8_decode
        fuzz/fuzz_utf8_decode.c
)
target_link_libraries(zireael_fuzz_utf8_decode PRIVATE zireael_build_settings)
target_link_libraries(zireael_fuzz_utf8_decode PRIVATE zireael)
add_test(NAME zireael.fuzz.utf8.decode COMMAND zireael_fuzz_utf8_decode)
set_tests_properties(zireael.fuzz.utf8.decode PROPERTIES TIMEOUT 5)

add_executable(zireael_fuzz_grapheme_iter
        fuzz/fuzz_grapheme_iter.c
)
target_link_libraries(zireael_fuzz_grapheme_iter PRIVATE zireael_build_settings)
target_link_libraries(zireael_fuzz_grapheme_iter PRIVATE zireael)
add_test(NAME zireael.fuzz.grapheme.iter COMMAND zireael_fuzz_grapheme_iter)
set_tests_properties(zireael.fuzz.grapheme.iter PROPERTIES TIMEOUT 5)
