//go:build !windows

package main

/*
  poc/go-codex-tui/zr_cgo_unix.go — cgo bridge for POSIX builds.

  Why: The demo exercises the engine as a wrapper would: build a drawlist byte
  stream, submit it, poll events, and query engine metrics.
*/

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../out/build/posix-clang-release -L${SRCDIR}/../../out/build/posix-clang-debug -lzireael -pthread

#include <stdint.h>
#include <stdlib.h>

#include <zr/zr_config.h>
#include <zr/zr_engine.h>
#include <zr/zr_metrics.h>
#include <zr/zr_result.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

type zrEngine struct {
	ptr *C.zr_engine_t
}

func zrDefaultConfig() zrEngineConfig { return zrEngineConfigDefault() }

func zrCreate(cfg *zrEngineConfig) (*zrEngine, error) {
	if cfg == nil {
		return nil, fmt.Errorf("config is nil")
	}

	c := C.zr_engine_config_t{
		requested_engine_abi_major:    C.uint32_t(cfg.requestedEngineABIMajor),
		requested_engine_abi_minor:    C.uint32_t(cfg.requestedEngineABIMinor),
		requested_engine_abi_patch:    C.uint32_t(cfg.requestedEngineABIPatch),
		requested_drawlist_version:    C.uint32_t(cfg.requestedDrawlistVersion),
		requested_event_batch_version: C.uint32_t(cfg.requestedEventBatchVer),
		limits: C.zr_limits_t{
			arena_max_total_bytes:    C.uint32_t(cfg.limits.arenaMaxTotalBytes),
			arena_initial_bytes:      C.uint32_t(cfg.limits.arenaInitialBytes),
			out_max_bytes_per_frame:  C.uint32_t(cfg.limits.outMaxBytesPerFrame),
			dl_max_total_bytes:       C.uint32_t(cfg.limits.dlMaxTotalBytes),
			dl_max_cmds:              C.uint32_t(cfg.limits.dlMaxCmds),
			dl_max_strings:           C.uint32_t(cfg.limits.dlMaxStrings),
			dl_max_blobs:             C.uint32_t(cfg.limits.dlMaxBlobs),
			dl_max_clip_depth:        C.uint32_t(cfg.limits.dlMaxClipDepth),
			dl_max_text_run_segments: C.uint32_t(cfg.limits.dlMaxTextRunSegments),
			diff_max_damage_rects:    C.uint32_t(cfg.limits.diffMaxDamageRects),
		},
		plat: C.plat_config_t{
			requested_color_mode:   C.plat_color_mode_t(cfg.plat.requestedColorMode),
			enable_mouse:           C.uint8_t(cfg.plat.enableMouse),
			enable_bracketed_paste: C.uint8_t(cfg.plat.enableBracketedPaste),
			enable_focus_events:    C.uint8_t(cfg.plat.enableFocusEvents),
			enable_osc52:           C.uint8_t(cfg.plat.enableOsc52),
			_pad:                   [3]C.uint8_t{C.uint8_t(cfg.plat._pad[0]), C.uint8_t(cfg.plat._pad[1]), C.uint8_t(cfg.plat._pad[2])},
		},
		tab_width:                   C.uint32_t(cfg.tabWidth),
		width_policy:                C.uint32_t(cfg.widthPolicy),
		target_fps:                  C.uint32_t(cfg.targetFPS),
		enable_scroll_optimizations: C.uint8_t(cfg.enableScrollOptimizations),
		enable_debug_overlay:        C.uint8_t(cfg.enableDebugOverlay),
		enable_replay_recording:     C.uint8_t(cfg.enableReplayRecording),
		wait_for_output_drain:       C.uint8_t(cfg.waitForOutputDrain),
	}

	var e *C.zr_engine_t
	rc := C.engine_create((**C.zr_engine_t)(unsafe.Pointer(&e)), &c)
	if rc != C.ZR_OK {
		return nil, fmt.Errorf("engine_create failed: %s", zrErrString(int32(rc)))
	}
	return &zrEngine{ptr: e}, nil
}

func (e *zrEngine) Destroy() {
	if e == nil || e.ptr == nil {
		return
	}
	C.engine_destroy(e.ptr)
	e.ptr = nil
}

func (e *zrEngine) PollEvents(timeoutMs int, out []byte) (int, error) {
	if e == nil || e.ptr == nil {
		return 0, fmt.Errorf("engine is nil")
	}
	var p *C.uint8_t
	if len(out) != 0 {
		p = (*C.uint8_t)(unsafe.Pointer(&out[0]))
	}
	n := C.engine_poll_events(e.ptr, C.int(timeoutMs), p, C.int(len(out)))
	if n < 0 {
		return 0, fmt.Errorf("engine_poll_events failed: %s", zrErrString(int32(n)))
	}
	return int(n), nil
}

func (e *zrEngine) SubmitDrawlist(dl []byte) error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("engine is nil")
	}
	if len(dl) == 0 {
		return fmt.Errorf("drawlist is empty")
	}
	rc := C.engine_submit_drawlist(e.ptr, (*C.uint8_t)(unsafe.Pointer(&dl[0])), C.int(len(dl)))
	if rc != C.ZR_OK {
		return fmt.Errorf("engine_submit_drawlist failed: %s", zrErrString(int32(rc)))
	}
	return nil
}

func (e *zrEngine) Present() error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("engine is nil")
	}
	rc := C.engine_present(e.ptr)
	if rc != C.ZR_OK {
		return fmt.Errorf("engine_present failed: %s", zrErrString(int32(rc)))
	}
	return nil
}

func (e *zrEngine) Metrics() (zrMetrics, error) {
	if e == nil || e.ptr == nil {
		return zrMetrics{}, fmt.Errorf("engine is nil")
	}
	var m C.zr_metrics_t
	m.struct_size = C.uint32_t(unsafe.Sizeof(m))
	rc := C.engine_get_metrics(e.ptr, &m)
	if rc != C.ZR_OK {
		return zrMetrics{}, fmt.Errorf("engine_get_metrics failed: %s", zrErrString(int32(rc)))
	}
	return zrMetrics{
		frameIndex:          uint64(m.frame_index),
		bytesEmittedTotal:   uint64(m.bytes_emitted_total),
		bytesEmittedLast:    uint32(m.bytes_emitted_last_frame),
		dirtyLinesLastFrame: uint32(m.dirty_lines_last_frame),
		dirtyColsLastFrame:  uint32(m.dirty_cols_last_frame),
	}, nil
}
