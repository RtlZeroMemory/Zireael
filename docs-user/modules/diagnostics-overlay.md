# Module: Diagnostics, metrics, debug overlay

Zireael exposes a stable metrics snapshot struct for wrappers and provides an
optional debug overlay for engine diagnostics.

## Source of truth

- Public metrics ABI: `include/zr/zr_metrics.h`
- Internal spec (normative): `docs/modules/DIAGNOSTICS_METRICS_DEBUG_OVERLAY.md`

The metrics struct is POD and append-only; callers should use the `struct_size`
prefix-copy convention described in the header.
