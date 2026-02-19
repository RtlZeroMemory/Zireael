/*
  tests/unit/mock_platform.h — OS-header-free mock platform for unit tests.

  Why: Provides a deterministic in-process implementation of the platform
  boundary so unit tests can exercise engine wiring (poll/present) without
  depending on a real terminal or OS backends.
*/

#ifndef ZR_TESTS_UNIT_MOCK_PLATFORM_H_INCLUDED
#define ZR_TESTS_UNIT_MOCK_PLATFORM_H_INCLUDED

#include "platform/zr_platform.h"
#include "util/zr_result.h"

#include <stddef.h>
#include <stdint.h>

void mock_plat_reset(void);

void mock_plat_set_size(uint32_t cols, uint32_t rows);
void mock_plat_set_caps(plat_caps_t caps);
void mock_plat_set_now_ms(uint64_t now_ms);
void mock_plat_set_output_writable(uint8_t writable);
void mock_plat_set_read_max(uint32_t max_bytes);
void mock_plat_set_terminal_query_support(uint8_t enabled);
void mock_plat_set_terminal_id_hint(zr_terminal_id_t id);

zr_result_t mock_plat_push_input(const uint8_t* bytes, size_t len);

void     mock_plat_clear_writes(void);
uint32_t mock_plat_write_call_count(void);
uint32_t mock_plat_wait_output_call_count(void);
uint64_t mock_plat_bytes_written_total(void);
size_t   mock_plat_last_write_len(void);
size_t   mock_plat_last_write_copy(uint8_t* out, size_t out_cap);

#endif /* ZR_TESTS_UNIT_MOCK_PLATFORM_H_INCLUDED */
