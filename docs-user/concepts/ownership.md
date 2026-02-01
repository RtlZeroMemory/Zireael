# Ownership & buffers

Zireael enforces an FFI-friendly ownership model:

- The engine owns all allocations it makes; callers never free engine memory.
- Callers provide buffers for:
  - drawlist bytes (`engine_submit_drawlist`)
  - packed event output (`engine_poll_events`)
  - user-event payload bytes (`engine_post_user_event`, copied during call)

The engine does not retain pointers into caller buffers beyond a call.
