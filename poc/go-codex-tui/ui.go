//go:build !windows

package main

import (
	"fmt"
	"strings"
	"time"
)

type rect struct {
	x int
	y int
	w int
	h int
}

func (r rect) inset(dx, dy int) rect {
	return rect{x: r.x + dx, y: r.y + dy, w: r.w - 2*dx, h: r.h - 2*dy}
}

func (r rect) clamp() rect {
	if r.w < 0 {
		r.w = 0
	}
	if r.h < 0 {
		r.h = 0
	}
	return r
}

func rgb(rr, gg, bb uint8) uint32 {
	return uint32(rr)<<16 | uint32(gg)<<8 | uint32(bb)
}

type theme struct {
	bg      uint32
	panel   uint32
	panel2  uint32
	text    uint32
	muted   uint32
	accent  uint32
	accent2 uint32
	good    uint32
	bad     uint32
}

func defaultTheme() theme {
	return theme{
		bg:      rgb(11, 16, 32),
		panel:   rgb(17, 26, 51),
		panel2:  rgb(25, 38, 74),
		text:    rgb(230, 235, 255),
		muted:   rgb(150, 162, 196),
		accent:  rgb(94, 234, 212),
		accent2: rgb(129, 140, 248),
		good:    rgb(74, 222, 128),
		bad:     rgb(248, 113, 113),
	}
}

func uiFill(b *dlBuilder, r rect, fg, bg uint32) {
	b.CmdFillRect(int32(r.x), int32(r.y), int32(r.w), int32(r.h), dlStyle{fg: fg, bg: bg, attrs: 0})
}

func uiText(b *dlBuilder, x, y int, s string, fg, bg uint32) {
	if s == "" {
		return
	}
	b.CmdDrawText(int32(x), int32(y), s, dlStyle{fg: fg, bg: bg, attrs: 0})
}

func uiTextClamp(b *dlBuilder, x, y, maxW int, s string, fg, bg uint32) {
	if maxW <= 0 || s == "" {
		return
	}
	uiText(b, x, y, truncateRunes(s, maxW), fg, bg)
}

func uiRule(b *dlBuilder, r rect, fg, bg uint32) {
	if r.w <= 0 || r.h <= 0 {
		return
	}
	line := strings.Repeat("─", r.w)
	uiText(b, r.x, r.y, line, fg, bg)
}

type uiRunSeg struct {
	text string
	fg   uint32
}

func uiTextRun(b *dlBuilder, x, y int, segs []uiRunSeg, bg uint32) {
	if len(segs) == 0 {
		return
	}

	run := make([]dlTextRunSeg, 0, len(segs))
	for i := 0; i < len(segs); i++ {
		if segs[i].text == "" {
			continue
		}
		idx := b.AddString(segs[i].text)
		run = append(run, dlTextRunSeg{
			style:       dlStyle{fg: segs[i].fg, bg: bg, attrs: 0},
			stringIndex: idx,
			byteOff:     0,
			byteLen:     uint32(len(segs[i].text)),
		})
	}
	if len(run) == 0 {
		return
	}

	blob := b.AddTextRunBlob(run)
	b.CmdDrawTextRun(int32(x), int32(y), blob)
}

func truncateRunes(s string, maxRunes int) string {
	if maxRunes <= 0 || s == "" {
		return ""
	}
	i := 0
	for j := range s {
		if i == maxRunes {
			return s[:j]
		}
		i++
	}
	return s
}

type perfSample struct {
	frameDur time.Duration
	fps      int

	zrBytesLast uint32
	zrDirtyCols uint32
	zrDirtyRows uint32

	goAlloc uint64
	goSys   uint64
}

func collectPerfSample(now time.Time, prevFrame time.Time, fps int, zrBytesLast, zrDirtyCols, zrDirtyRows uint32,
	goAlloc uint64, goSys uint64) perfSample {
	return perfSample{
		frameDur:    now.Sub(prevFrame),
		fps:         fps,
		zrBytesLast: zrBytesLast,
		zrDirtyCols: zrDirtyCols,
		zrDirtyRows: zrDirtyRows,
		goAlloc:     goAlloc,
		goSys:       goSys,
	}
}

func uiMetricsPanel(b *dlBuilder, r rect, th theme, p perfSample, stressCmds int) {
	uiFill(b, r, th.text, th.panel)

	y := r.y + 1
	uiTextClamp(b, r.x+2, y, r.w-4, "Performance", th.accent, th.panel)
	y += 2

	uiTextClamp(b, r.x+2, y, r.w-4, fmt.Sprintf("FPS: %d", p.fps), th.text, th.panel)
	y++
	uiTextClamp(b, r.x+2, y, r.w-4, fmt.Sprintf("Frame: %.2f ms", float64(p.frameDur)/float64(time.Millisecond)), th.muted, th.panel)
	y += 2

	uiTextClamp(b, r.x+2, y, r.w-4, fmt.Sprintf("Zr bytes/frame: %d", p.zrBytesLast), th.text, th.panel)
	y++
	uiTextClamp(b, r.x+2, y, r.w-4, fmt.Sprintf("Zr dirty cols: %d", p.zrDirtyCols), th.muted, th.panel)
	y++
	uiTextClamp(b, r.x+2, y, r.w-4, fmt.Sprintf("Zr dirty rows: %d", p.zrDirtyRows), th.muted, th.panel)
	y += 2

	uiTextClamp(b, r.x+2, y, r.w-4, fmt.Sprintf("Go alloc: %.1f MiB", float64(p.goAlloc)/(1024.0*1024.0)), th.text, th.panel)
	y++
	uiTextClamp(b, r.x+2, y, r.w-4, fmt.Sprintf("Go sys: %.1f MiB", float64(p.goSys)/(1024.0*1024.0)), th.muted, th.panel)
	y += 2

	uiTextClamp(b, r.x+2, y, r.w-4, fmt.Sprintf("Stress cmds: %d", stressCmds), th.accent2, th.panel)
}
