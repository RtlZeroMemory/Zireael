# API overview

Zireael’s public surface is intentionally small.

The primary entrypoints are declared in `include/zr/zr_engine.h`:

- `engine_create` / `engine_destroy`
- `engine_poll_events` / `engine_post_user_event`
- `engine_submit_drawlist` / `engine_present`
- `engine_get_metrics` / `engine_set_config`

Threading:

- `engine_post_user_event()` is thread-safe.
- all other `engine_*` calls are engine-thread only.
