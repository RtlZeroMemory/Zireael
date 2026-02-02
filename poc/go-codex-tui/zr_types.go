package main

/*
  poc/go-codex-tui/zr_types.go — Go-side mirror of the public Zireael ABI types.

  Why: The demo is meant to act like a wrapper. Keeping a Go-native mirror of
  the config/metrics structs lets us support both:
    - POSIX via cgo (linking static library)
    - Windows via DLL calls (no cgo toolchain requirement)
*/

const (
	zrOK                = 0
	zrErrInvalidArg     = -1
	zrErrOOM            = -2
	zrErrLimit          = -3
	zrErrUnsupported    = -4
	zrErrFormat         = -5
	zrErrPlatform       = -6
	zrEngineABIMajor    = 1
	zrEngineABIMinor    = 0
	zrEngineABIPatch    = 0
	zrDrawlistVersionV1 = 1
	zrEventBatchV1      = 1
)

const (
	platColorUnknown uint8 = 0
	platColor16      uint8 = 1
	platColor256     uint8 = 2
	platColorRGB     uint8 = 3
)

type zrLimits struct {
	arenaMaxTotalBytes   uint32
	arenaInitialBytes    uint32
	outMaxBytesPerFrame  uint32
	dlMaxTotalBytes      uint32
	dlMaxCmds            uint32
	dlMaxStrings         uint32
	dlMaxBlobs           uint32
	dlMaxClipDepth       uint32
	dlMaxTextRunSegments uint32
	diffMaxDamageRects   uint32
}

type zrPlatConfig struct {
	requestedColorMode   uint8
	enableMouse          uint8
	enableBracketedPaste uint8
	enableFocusEvents    uint8
	enableOsc52          uint8
	_pad                 [3]uint8
}

type zrEngineConfig struct {
	requestedEngineABIMajor  uint32
	requestedEngineABIMinor  uint32
	requestedEngineABIPatch  uint32
	requestedDrawlistVersion uint32
	requestedEventBatchVer   uint32

	limits zrLimits
	plat   zrPlatConfig

	tabWidth    uint32
	widthPolicy uint32
	targetFPS   uint32

	enableScrollOptimizations uint8
	enableDebugOverlay        uint8
	enableReplayRecording     uint8
	waitForOutputDrain        uint8
}

type zrMetrics struct {
	frameIndex          uint64
	bytesEmittedTotal   uint64
	bytesEmittedLast    uint32
	dirtyLinesLastFrame uint32
	dirtyColsLastFrame  uint32
}

func zrEngineConfigDefault() zrEngineConfig {
	/*
		Defaults should match the engine's pinned defaults (see src/core/zr_config.c
		and src/util/zr_caps.c). The demo overrides limits aggressively for stress.
	*/
	return zrEngineConfig{
		requestedEngineABIMajor:  zrEngineABIMajor,
		requestedEngineABIMinor:  zrEngineABIMinor,
		requestedEngineABIPatch:  zrEngineABIPatch,
		requestedDrawlistVersion: zrDrawlistVersionV1,
		requestedEventBatchVer:   zrEventBatchV1,
		limits: zrLimits{
			arenaMaxTotalBytes:   4 * 1024 * 1024,
			arenaInitialBytes:    64 * 1024,
			outMaxBytesPerFrame:  256 * 1024,
			dlMaxTotalBytes:      256 * 1024,
			dlMaxCmds:            4096,
			dlMaxStrings:         4096,
			dlMaxBlobs:           4096,
			dlMaxClipDepth:       64,
			dlMaxTextRunSegments: 4096,
			diffMaxDamageRects:   4096,
		},
		plat: zrPlatConfig{
			requestedColorMode:   platColorUnknown,
			enableMouse:          1,
			enableBracketedPaste: 1,
			enableFocusEvents:    1,
			enableOsc52:          0,
		},
		tabWidth:                  4,
		widthPolicy:               1, // ZR_WIDTH_EMOJI_WIDE (pinned default)
		targetFPS:                 60,
		enableScrollOptimizations: 1,
		enableDebugOverlay:        0,
		enableReplayRecording:     0,
		waitForOutputDrain:        0,
	}
}
