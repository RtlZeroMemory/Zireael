//go:build windows

package main

import (
	"syscall"
	"unsafe"
)

type winCoord struct {
	x int16
	y int16
}

type winSmallRect struct {
	left   int16
	top    int16
	right  int16
	bottom int16
}

type winConsoleScreenBufferInfo struct {
	size              winCoord
	cursorPosition    winCoord
	attributes        uint16
	window            winSmallRect
	maximumWindowSize winCoord
}

func ttySize(fd uintptr) (cols, rows int, ok bool) {
	k32 := syscall.NewLazyDLL("kernel32.dll")
	getInfo := k32.NewProc("GetConsoleScreenBufferInfo")
	getStd := k32.NewProc("GetStdHandle")
	createFileW := k32.NewProc("CreateFileW")
	closeHandle := k32.NewProc("CloseHandle")

	tryHandle := func(h uintptr) (int, int, bool) {
		if h == 0 || h == ^uintptr(0) {
			return 0, 0, false
		}
		var info winConsoleScreenBufferInfo
		r1, _, _ := getInfo.Call(h, uintptr(unsafe.Pointer(&info)))
		if r1 == 0 {
			return 0, 0, false
		}
		c := int(info.window.right-info.window.left) + 1
		r := int(info.window.bottom-info.window.top) + 1
		if c <= 0 || r <= 0 {
			return 0, 0, false
		}
		return c, r, true
	}

	/* First: caller-provided handle (often os.Stdout.Fd()). */
	if c, r, ok2 := tryHandle(fd); ok2 {
		return c, r, true
	}

	/* Second: STDOUT handle directly (can differ under some hosts). */
	const stdOutputHandle = uintptr(^uint32(10)) // (DWORD)-11
	hOut, _, _ := getStd.Call(stdOutputHandle)
	if c, r, ok2 := tryHandle(hOut); ok2 {
		return c, r, true
	}

	/* Third: open CONOUT$ (works in many pseudo console configurations). */
	conout, _ := syscall.UTF16PtrFromString("CONOUT$")
	const (
		genericRead  = 0x80000000
		genericWrite = 0x40000000
		openExisting = 3
		fileShareRW  = 0x00000003
	)
	h, _, _ := createFileW.Call(
		uintptr(unsafe.Pointer(conout)),
		uintptr(genericRead|genericWrite),
		uintptr(fileShareRW),
		0,
		uintptr(openExisting),
		0,
		0,
	)
	if c, r, ok2 := tryHandle(h); ok2 {
		_, _, _ = closeHandle.Call(h)
		return c, r, true
	}
	if h != 0 && h != ^uintptr(0) {
		_, _, _ = closeHandle.Call(h)
	}

	return 0, 0, false
}
