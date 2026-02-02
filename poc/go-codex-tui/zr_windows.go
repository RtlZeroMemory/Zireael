//go:build windows

package main

/*
  poc/go-codex-tui/zr_windows.go — Windows engine calls via DLL (no cgo).

  Why: A Windows-native demo should run without requiring a separate cgo toolchain.
  We build `zireael.dll` via the existing CMake presets and call the public ABI
  through kernel32's DLL loader.
*/

import (
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"syscall"
	"unsafe"
)

type zrEngine struct {
	ptr uintptr
}

type zrMetricsRaw struct {
	structSize uint32

	negEngineABIMajor uint32
	negEngineABIMinor uint32
	negEngineABIPatch uint32

	negDrawlistVersion   uint32
	negEventBatchVersion uint32

	frameIndex uint64
	fps        uint32
	_pad0      uint32

	bytesEmittedTotal     uint64
	bytesEmittedLastFrame uint32
	_pad1                 uint32

	dirtyLinesLastFrame uint32
	dirtyColsLastFrame  uint32

	usInputLastFrame    uint32
	usDrawlistLastFrame uint32
	usDiffLastFrame     uint32
	usWriteLastFrame    uint32

	eventsOutLastPoll  uint32
	eventsDroppedTotal uint32

	arenaFrameHighWaterBytes      uint64
	arenaPersistentHighWaterBytes uint64
}

var (
	zrOnce sync.Once
	zrDLL  *syscall.DLL

	procCreate     *syscall.Proc
	procDestroy    *syscall.Proc
	procPollEvents *syscall.Proc
	procSubmitDL   *syscall.Proc
	procPresent    *syscall.Proc
	procGetMetrics *syscall.Proc
	zrInitErr      error
)

func zrDefaultConfig() zrEngineConfig { return zrEngineConfigDefault() }

func zrInitDLL() error {
	zrOnce.Do(func() {
		path, err := zrFindDLLPath()
		if err != nil {
			zrInitErr = err
			return
		}
		d, derr := syscall.LoadDLL(path)
		if derr != nil {
			zrInitErr = fmt.Errorf("load zireael dll (%s): %w", path, derr)
			return
		}
		zrDLL = d

		find := func(name string) *syscall.Proc {
			p, e := zrDLL.FindProc(name)
			if e != nil && zrInitErr == nil {
				zrInitErr = fmt.Errorf("find proc %s: %w", name, e)
			}
			return p
		}

		procCreate = find("engine_create")
		procDestroy = find("engine_destroy")
		procPollEvents = find("engine_poll_events")
		procSubmitDL = find("engine_submit_drawlist")
		procPresent = find("engine_present")
		procGetMetrics = find("engine_get_metrics")
	})
	return zrInitErr
}

func zrFindDLLPath() (string, error) {
	if p := os.Getenv("ZR_DLL_PATH"); p != "" {
		ap, err := filepath.Abs(p)
		if err != nil {
			return "", err
		}
		return ap, nil
	}

	rel := filepath.FromSlash("out/build/windows-clangcl-debug/zireael.dll")
	rel2 := filepath.FromSlash("out/build/windows-clangcl-release/zireael.dll")

	wd, _ := os.Getwd()
	candidates := []string{}
	for up := 0; up <= 5; up++ {
		base := wd
		for i := 0; i < up; i++ {
			base = filepath.Dir(base)
		}
		candidates = append(candidates, filepath.Join(base, rel))
		candidates = append(candidates, filepath.Join(base, rel2))
	}

	for _, p := range candidates {
		if st, err := os.Stat(p); err == nil && !st.IsDir() {
			return p, nil
		}
	}
	return "", fmt.Errorf("zireael.dll not found (set ZR_DLL_PATH or build via CMake presets)")
}

func zrCreate(cfg *zrEngineConfig) (*zrEngine, error) {
	if cfg == nil {
		return nil, fmt.Errorf("config is nil")
	}
	if err := zrInitDLL(); err != nil {
		return nil, err
	}
	if procCreate == nil {
		return nil, fmt.Errorf("engine_create proc missing")
	}

	var ePtr uintptr
	r1, _, _ := procCreate.Call(
		uintptr(unsafe.Pointer(&ePtr)),
		uintptr(unsafe.Pointer(cfg)),
	)
	rc := int32(r1)
	if rc != zrOK {
		return nil, fmt.Errorf("engine_create failed: %s", zrErrString(rc))
	}
	return &zrEngine{ptr: ePtr}, nil
}

func (e *zrEngine) Destroy() {
	if e == nil || e.ptr == 0 || procDestroy == nil {
		return
	}
	procDestroy.Call(e.ptr)
	e.ptr = 0
}

func (e *zrEngine) PollEvents(timeoutMs int, out []byte) (int, error) {
	if e == nil || e.ptr == 0 || procPollEvents == nil {
		return 0, fmt.Errorf("engine is nil")
	}
	var outPtr uintptr
	if len(out) != 0 {
		outPtr = uintptr(unsafe.Pointer(&out[0]))
	}
	r1, _, _ := procPollEvents.Call(
		e.ptr,
		uintptr(int32(timeoutMs)),
		outPtr,
		uintptr(int32(len(out))),
	)
	n := int32(r1)
	if n < 0 {
		return 0, fmt.Errorf("engine_poll_events failed: %s", zrErrString(n))
	}
	return int(n), nil
}

func (e *zrEngine) SubmitDrawlist(dl []byte) error {
	if e == nil || e.ptr == 0 || procSubmitDL == nil {
		return fmt.Errorf("engine is nil")
	}
	if len(dl) == 0 {
		return fmt.Errorf("drawlist is empty")
	}
	r1, _, _ := procSubmitDL.Call(
		e.ptr,
		uintptr(unsafe.Pointer(&dl[0])),
		uintptr(int32(len(dl))),
	)
	rc := int32(r1)
	if rc != zrOK {
		return fmt.Errorf("engine_submit_drawlist failed: %s", zrErrString(rc))
	}
	return nil
}

func (e *zrEngine) Present() error {
	if e == nil || e.ptr == 0 || procPresent == nil {
		return fmt.Errorf("engine is nil")
	}
	r1, _, _ := procPresent.Call(e.ptr)
	rc := int32(r1)
	if rc != zrOK {
		return fmt.Errorf("engine_present failed: %s", zrErrString(rc))
	}
	return nil
}

func (e *zrEngine) Metrics() (zrMetrics, error) {
	if e == nil || e.ptr == 0 || procGetMetrics == nil {
		return zrMetrics{}, fmt.Errorf("engine is nil")
	}
	var raw zrMetricsRaw
	raw.structSize = uint32(unsafe.Sizeof(raw))

	r1, _, _ := procGetMetrics.Call(
		e.ptr,
		uintptr(unsafe.Pointer(&raw)),
	)
	rc := int32(r1)
	if rc != zrOK {
		return zrMetrics{}, fmt.Errorf("engine_get_metrics failed: %s", zrErrString(rc))
	}
	return zrMetrics{
		frameIndex:          raw.frameIndex,
		bytesEmittedTotal:   raw.bytesEmittedTotal,
		bytesEmittedLast:    raw.bytesEmittedLastFrame,
		dirtyLinesLastFrame: raw.dirtyLinesLastFrame,
		dirtyColsLastFrame:  raw.dirtyColsLastFrame,
	}, nil
}
