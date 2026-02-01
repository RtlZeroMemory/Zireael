/*
  tests/zr_tests_main.c — Unit + golden test binary entrypoint.

  Why: Single portable test executable that CTest can invoke with filters to run
  unit or golden test subsets deterministically.
*/

#include "zr_test.h"

int main(int argc, char** argv) {
  return zr_test_run_all(argc, argv);
}

