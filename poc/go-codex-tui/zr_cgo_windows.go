//go:build windows

package main

/*
  poc/go-codex-tui/zr_cgo_windows.go — Build guard for the Go PoC on Windows.

  Why: This PoC links against a locally-built Zireael static library via cgo.
  The repo's primary engine support for Windows is clang-cl + CMake presets, but
  this demo runner is currently wired for POSIX builds only.
*/

import "errors"

var errWindowsUnsupported = errors.New("poc/go-codex-tui: Windows runner not wired yet (use POSIX for now)")
