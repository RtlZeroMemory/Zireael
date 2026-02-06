/*
  tests/fuzz/libfuzzer_drawlist_parser.c — Coverage-guided drawlist validator harness.

  Why: Supplements deterministic smoke fuzzing with libFuzzer coverage guidance
  to explore parser edge cases beyond fixed-seed random generation.
*/

#include "core/zr_drawlist.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void zr_fuzz_trap(void) {
#if defined(_MSC_VER)
  __debugbreak();
#elif defined(__GNUC__) || defined(__clang__)
  __builtin_trap();
#else
  volatile int* p = (volatile int*)0;
  *p = 0;
#endif
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  zr_limits_t lim = zr_limits_default();
  lim.dl_max_total_bytes = (size > (size_t)UINT32_MAX) ? UINT32_MAX : (uint32_t)size;
  lim.dl_max_cmds = 256u;
  lim.dl_max_strings = 256u;
  lim.dl_max_blobs = 256u;
  lim.dl_max_clip_depth = 64u;
  lim.dl_max_text_run_segments = 256u;

  zr_dl_view_t v1;
  zr_dl_view_t v2;
  const zr_result_t r1 = zr_dl_validate(data, size, &lim, &v1);
  const zr_result_t r2 = zr_dl_validate(data, size, &lim, &v2);
  if (r1 != r2) {
    zr_fuzz_trap();
  }
  if (r1 == ZR_OK) {
    if (memcmp(&v1.hdr, &v2.hdr, sizeof(v1.hdr)) != 0 || v1.cmd_bytes_len != v2.cmd_bytes_len ||
        v1.strings_count != v2.strings_count || v1.blobs_count != v2.blobs_count) {
      zr_fuzz_trap();
    }
  }
  return 0;
}
