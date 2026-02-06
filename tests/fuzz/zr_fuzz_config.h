/*
  tests/fuzz/zr_fuzz_config.h — Shared deterministic fuzz smoke configuration.

  Why: Allows CI/nightly jobs to scale smoke fuzz iteration budgets via
  environment variables without forking harness logic per target.
*/

#ifndef ZR_FUZZ_CONFIG_H_INCLUDED
#define ZR_FUZZ_CONFIG_H_INCLUDED

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

/*
  Read a bounded positive integer from environment.

  Why: Keeps smoke fuzz runs deterministic while allowing controlled budget
  scaling in CI via `ZR_FUZZ_ITERS` and `ZR_FUZZ_MAX_SIZE`.
*/
static inline int zr_fuzz_env_int(const char* key, int fallback, int min_value, int max_value) {
  if (!key || fallback < min_value || fallback > max_value || min_value > max_value) {
    return fallback;
  }

  const char* value = getenv(key);
  if (!value || value[0] == '\0') {
    return fallback;
  }

  errno = 0;
  char* end = NULL;
  long parsed = strtol(value, &end, 10);
  if (errno != 0 || !end || end == value || *end != '\0') {
    return fallback;
  }
  if (parsed < (long)min_value || parsed > (long)max_value) {
    return fallback;
  }
  return (int)parsed;
}

#endif /* ZR_FUZZ_CONFIG_H_INCLUDED */
