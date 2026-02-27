/*
  tests/golden/zr_golden.h — Golden fixture loader + byte-for-byte comparator.

  Why: Enables deterministic golden tests by comparing actual output bytes against
  canonical `expected.bin` fixtures with actionable mismatch diagnostics.
*/

#ifndef ZR_GOLDEN_H_INCLUDED
#define ZR_GOLDEN_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

/*
  Compares `actual` bytes against:
    tests/golden/fixtures/<fixture_id>/expected.bin

  Returns:
    0 on exact match, non-zero on failure (mismatch/missing/read error).
*/
int zr_golden_compare_fixture(const char* fixture_id, const uint8_t* actual, size_t actual_len);

#endif /* ZR_GOLDEN_H_INCLUDED */
