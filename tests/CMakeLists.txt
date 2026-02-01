add_executable(zireael_tests_unit
  unit/unit_smoke.c
)

target_link_libraries(zireael_tests_unit PRIVATE zireael)

add_test(NAME zireael.unit COMMAND zireael_tests_unit)

