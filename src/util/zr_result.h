/*
  src/util/zr_result.h — Shared result code type and error constants.

  Why: Establishes Zireael's cross-module "0 = OK, negative = failure" contract.
*/

#ifndef ZR_UTIL_ZR_RESULT_H_INCLUDED
#define ZR_UTIL_ZR_RESULT_H_INCLUDED

typedef int zr_result_t;

/* Success. */
#define ZR_OK ((zr_result_t)0)

/*
  Failures (negative).

  Note: Numeric values are intended to match the project's error catalog.
*/
#define ZR_ERR_INVALID_ARGUMENT ((zr_result_t)-1)
#define ZR_ERR_OOM ((zr_result_t)-2)
#define ZR_ERR_LIMIT ((zr_result_t)-3)
#define ZR_ERR_UNSUPPORTED ((zr_result_t)-4)
#define ZR_ERR_FORMAT ((zr_result_t)-5)

#endif /* ZR_UTIL_ZR_RESULT_H_INCLUDED */
