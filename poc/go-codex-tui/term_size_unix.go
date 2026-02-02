//go:build (linux || darwin) && !windows

package main

import (
	"syscall"
	"unsafe"
)

type ttyWinSize struct {
	row    uint16
	col    uint16
	xpixel uint16
	ypixel uint16
}

func ttySize(fd uintptr) (cols, rows int, ok bool) {
	var ws ttyWinSize
	_, _, errno := syscall.Syscall(syscall.SYS_IOCTL, fd, uintptr(syscall.TIOCGWINSZ), uintptr(unsafe.Pointer(&ws)))
	if errno != 0 {
		return 0, 0, false
	}
	if ws.col == 0 || ws.row == 0 {
		return 0, 0, false
	}
	return int(ws.col), int(ws.row), true
}

