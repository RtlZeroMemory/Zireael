//go:build !windows

package main

/*
  poc/go-codex-tui/zr_cgo.go — Minimal cgo bridge for the Zireael engine ABI.

  Why: The demo is intended to exercise Zireael as a "wrapper" would: build a
  drawlist byte stream, submit it, poll events, and display engine metrics.
*/

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../out/build/posix-clang-debug -lzireael -pthread

#include <stdint.h>
#include <stdlib.h>

#include <zr/zr_config.h>
#include <zr/zr_drawlist.h>
#include <zr/zr_engine.h>
#include <zr/zr_event.h>
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

func zrErrString(rc C.zr_result_t) string {
	switch rc {
	case C.ZR_OK:
		return "ZR_OK"
	case C.ZR_ERR_INVALID_ARGUMENT:
		return "ZR_ERR_INVALID_ARGUMENT"
	case C.ZR_ERR_OOM:
		return "ZR_ERR_OOM"
	case C.ZR_ERR_LIMIT:
		return "ZR_ERR_LIMIT"
	case C.ZR_ERR_UNSUPPORTED:
		return "ZR_ERR_UNSUPPORTED"
	case C.ZR_ERR_FORMAT:
		return "ZR_ERR_FORMAT"
	case C.ZR_ERR_PLATFORM:
		return "ZR_ERR_PLATFORM"
	default:
		return fmt.Sprintf("ZR_ERR_%d", int32(rc))
	}
}

func zrDefaultConfig() C.zr_engine_config_t {
	return C.zr_engine_config_default()
}

func zrCreate(cfg *C.zr_engine_config_t) (*zrEngine, error) {
	var e *C.zr_engine_t
	rc := C.engine_create((**C.zr_engine_t)(unsafe.Pointer(&e)), cfg)
	if rc != C.ZR_OK {
		return nil, fmt.Errorf("engine_create failed: %s", zrErrString(rc))
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
		return 0, fmt.Errorf("engine_poll_events failed: %s", zrErrString(C.zr_result_t(n)))
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
		return fmt.Errorf("engine_submit_drawlist failed: %s", zrErrString(rc))
	}
	return nil
}

func (e *zrEngine) Present() error {
	if e == nil || e.ptr == nil {
		return fmt.Errorf("engine is nil")
	}
	rc := C.engine_present(e.ptr)
	if rc != C.ZR_OK {
		return fmt.Errorf("engine_present failed: %s", zrErrString(rc))
	}
	return nil
}

func (e *zrEngine) Metrics() (C.zr_metrics_t, error) {
	if e == nil || e.ptr == nil {
		return C.zr_metrics_t{}, fmt.Errorf("engine is nil")
	}
	var m C.zr_metrics_t
	m.struct_size = C.uint32_t(unsafe.Sizeof(m))
	rc := C.engine_get_metrics(e.ptr, &m)
	if rc != C.ZR_OK {
		return C.zr_metrics_t{}, fmt.Errorf("engine_get_metrics failed: %s", zrErrString(rc))
	}
	return m, nil
}
