/*
  tests/zr_test.h — Minimal deterministic unit test harness API.

  Why: Provides a tiny, portable runner with assertions so CTest can execute
  deterministic unit + golden tests without OS/terminal dependencies.
*/

#ifndef ZR_TEST_H_INCLUDED
#define ZR_TEST_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

typedef struct zr_test_ctx_t zr_test_ctx_t;
typedef void (*zr_test_fn_t)(zr_test_ctx_t* ctx);

typedef struct zr_test_case_t {
  const char* name;
  zr_test_fn_t fn;
} zr_test_case_t;

/* Registration / runner. */
void zr_test_register(const char* name, zr_test_fn_t fn);
int zr_test_run_all(int argc, char** argv);

/* Internals used by assertion macros. */
void zr_test_fail(zr_test_ctx_t* ctx, const char* file, int line, const char* msg);
void zr_test_failf(zr_test_ctx_t* ctx, const char* file, int line, const char* fmt, ...);
void zr_test_skip(zr_test_ctx_t* ctx, const char* file, int line, const char* reason);

/* Test definition + auto-registration (portable across clang/gcc/MSVC/clang-cl). */
#if defined(_MSC_VER)
typedef void(__cdecl* zr_test_ctor_fn_t)(void);
#pragma section(".CRT$XCU", read)

#if defined(__clang__)
#define ZR_TEST_USED_ATTR __attribute__((used))
#else
#define ZR_TEST_USED_ATTR
#endif

#define ZR_TEST_NAMED(ident, name_str)                                                                                 \
  static void ident(zr_test_ctx_t* ctx);                                                                               \
  static void __cdecl zr_test_reg__##ident(void);                                                                      \
  ZR_TEST_USED_ATTR __declspec(allocate(".CRT$XCU")) static const zr_test_ctor_fn_t zr_test_ctor__##ident =            \
      zr_test_reg__##ident;                                                                                            \
  static void __cdecl zr_test_reg__##ident(void) {                                                                     \
    zr_test_register((name_str), ident);                                                                               \
  }                                                                                                                    \
  static void ident(zr_test_ctx_t* ctx)
#elif defined(__GNUC__) || defined(__clang__)
#define ZR_TEST_NAMED(ident, name_str)                                                                                 \
  static void ident(zr_test_ctx_t* ctx);                                                                               \
  static void zr_test_reg__##ident(void) __attribute__((constructor));                                                 \
  static void zr_test_reg__##ident(void) {                                                                             \
    zr_test_register((name_str), ident);                                                                               \
  }                                                                                                                    \
  static void ident(zr_test_ctx_t* ctx)
#else
#error "Unsupported compiler for ZR_TEST auto-registration."
#endif

#define ZR_TEST(ident) ZR_TEST_NAMED(ident, #ident)

/* Convenience naming for CTest --prefix filters. */
#define ZR_TEST_UNIT(name) ZR_TEST_NAMED(unit__##name, "unit." #name)
#define ZR_TEST_GOLDEN(name) ZR_TEST_NAMED(golden__##name, "golden." #name)

/* Assertions. */
#define ZR_ASSERT_TRUE(cond)                                                                                           \
  do {                                                                                                                 \
    if (!(cond)) {                                                                                                     \
      zr_test_fail((ctx), __FILE__, __LINE__, "ZR_ASSERT_TRUE failed: " #cond);                                        \
      return;                                                                                                          \
    }                                                                                                                  \
  } while (0)

#define ZR_ASSERT_EQ_U32(a, b)                                                                                         \
  do {                                                                                                                 \
    const uint32_t _zr_a = (uint32_t)(a);                                                                              \
    const uint32_t _zr_b = (uint32_t)(b);                                                                              \
    if (_zr_a != _zr_b) {                                                                                              \
      zr_test_failf((ctx), __FILE__, __LINE__, "ZR_ASSERT_EQ_U32 failed: %s=%u %s=%u", #a, (unsigned)_zr_a, #b,        \
                    (unsigned)_zr_b);                                                                                  \
      return;                                                                                                          \
    }                                                                                                                  \
  } while (0)

/*
  ZR_ASSERT_MEMEQ:
    - compares byte-for-byte
    - on mismatch prints first mismatch offset and byte context (hex)
*/
#define ZR_ASSERT_MEMEQ(actual_ptr, expected_ptr, len)                                                                 \
  do {                                                                                                                 \
    const void* _zr_a_ptr = (actual_ptr);                                                                              \
    const void* _zr_e_ptr = (expected_ptr);                                                                            \
    const size_t _zr_len = (size_t)(len);                                                                              \
    if (!zr_test_memeq(_zr_a_ptr, _zr_e_ptr, _zr_len)) {                                                               \
      zr_test_failf((ctx), __FILE__, __LINE__, "ZR_ASSERT_MEMEQ failed: %s vs %s (len=%zu)", #actual_ptr,              \
                    #expected_ptr, _zr_len);                                                                           \
      return;                                                                                                          \
    }                                                                                                                  \
  } while (0)

/* Returns 1 if equal (and prints diagnostics if not). */
int zr_test_memeq(const void* actual, const void* expected, size_t len);

/* Deterministic skip (useful for integration test scaffolding). */
#define ZR_SKIP(reason)                                                                                                \
  do {                                                                                                                 \
    zr_test_skip((ctx), __FILE__, __LINE__, (reason));                                                                 \
    return;                                                                                                            \
  } while (0)

#endif /* ZR_TEST_H_INCLUDED */
