/*
  include/zr/zr_result.h — Public result type and error constants.

  Why: Establishes Zireael's cross-module "0 = OK, negative = failure" contract
  in a header-safe form for wrappers and native callers.
*/

#ifndef ZR_ZR_RESULT_H_INCLUDED
#define ZR_ZR_RESULT_H_INCLUDED

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
#define ZR_ERR_PLATFORM ((zr_result_t)-6)

#endif /* ZR_ZR_RESULT_H_INCLUDED */
