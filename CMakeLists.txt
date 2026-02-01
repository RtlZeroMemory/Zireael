cmake_minimum_required(VERSION 3.21)

project(Zireael LANGUAGES C)

option(ZIREAEL_BUILD_SHARED "Build shared library (optional)" OFF)
option(ZIREAEL_BUILD_EXAMPLES "Build examples" ON)
option(ZIREAEL_BUILD_TESTS "Build tests" ON)

add_library(zireael STATIC
  src/core/zr_placeholder.c
)
add_library(Zireael::zireael ALIAS zireael)

set_target_properties(zireael PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED YES
  C_EXTENSIONS NO
)

target_include_directories(zireael PUBLIC
  ${PROJECT_SOURCE_DIR}/src
)

if (ZIREAEL_BUILD_SHARED)
  add_library(zireael_shared SHARED
    src/core/zr_placeholder.c
  )
  add_library(Zireael::zireael_shared ALIAS zireael_shared)

  set_target_properties(zireael_shared PROPERTIES
    OUTPUT_NAME zireael
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
  )

  target_include_directories(zireael_shared PUBLIC
    ${PROJECT_SOURCE_DIR}/src
  )
endif ()

if (ZIREAEL_BUILD_EXAMPLES)
  add_executable(zireael_example
    examples/example.c
  )
  target_link_libraries(zireael_example PRIVATE zireael)
endif ()

if (ZIREAEL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif ()

