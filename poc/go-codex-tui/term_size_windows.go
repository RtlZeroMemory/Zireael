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
	if fd == 0 {
		return 0, 0, false
	}

	k32 := syscall.NewLazyDLL("kernel32.dll")
	getInfo := k32.NewProc("GetConsoleScreenBufferInfo")

	var info winConsoleScreenBufferInfo
	r1, _, _ := getInfo.Call(fd, uintptr(unsafe.Pointer(&info)))
	if r1 == 0 {
		return 0, 0, false
	}

	cols = int(info.window.right-info.window.left) + 1
	rows = int(info.window.bottom-info.window.top) + 1
	if cols <= 0 || rows <= 0 {
		return 0, 0, false
	}
	return cols, rows, true
}
