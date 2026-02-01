add_executable(zireael_unit_tests
  zr_test.c
  zr_tests_main.c
  unit/unit_smoke.c
  unit/test_arena.c
  unit/test_bytes.c
  unit/test_checked.c
  unit/test_drawlist_execute.c
  unit/test_drawlist_validate.c
  unit/test_limits.c
  unit/test_ring.c
  unit/test_string_builder.c
  unit/test_vec.c
  golden/zr_golden.c
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

add_executable(zireael_integration_tests
  integration/zr_integration_smoke.c
)
target_link_libraries(zireael_integration_tests PRIVATE zireael_build_settings)
target_link_libraries(zireael_integration_tests PRIVATE zireael)
add_test(NAME zireael.integration COMMAND zireael_integration_tests)
set_tests_properties(zireael.integration PROPERTIES SKIP_RETURN_CODE 77)

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
