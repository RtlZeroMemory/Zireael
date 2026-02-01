//go:build !windows

package main

import (
	"fmt"
	"strings"
	"time"
)

type scenarioID uint8

const (
	scenarioThinking scenarioID = iota
	scenarioMatrix
	scenarioStorm
)

type scenarioDef struct {
	id   scenarioID
	name string
	desc string
}

var scenarios = []scenarioDef{
	{scenarioThinking, "Codex Thinking", "A Codex/Claude-like workflow view with animated \"thinking\" output."},
	{scenarioMatrix, "Matrix Rain", "Fullscreen green noise with UI chrome and perf overlay."},
	{scenarioStorm, "Element Storm", "Draws N single-cell elements per frame (stress test)."},
}

type thinkingState struct {
	start time.Time
}

func (s *thinkingState) Reset(now time.Time) {
	s.start = now
}

func (s *thinkingState) Draw(b *dlBuilder, r rect, th theme, now time.Time) {
	uiFill(b, r, th.text, th.bg)

	card := r.inset(2, 1).clamp()
	if card.w < 10 || card.h < 8 {
		return
	}
	uiFill(b, card, th.text, th.panel)
	uiRule(b, rect{x: card.x + 2, y: card.y + 2, w: card.w - 4, h: 1}, th.panel2, th.panel)

	title := "Zireael • Go PoC • Codex Mode"
	uiTextClamp(b, card.x+2, card.y+1, card.w-4, title, th.accent, th.panel)

	elapsed := now.Sub(s.start)
	spinner := []string{"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"}
	spin := spinner[int(elapsed/(120*time.Millisecond))%len(spinner)]

	uiTextClamp(b, card.x+2, card.y+3, card.w-4, fmt.Sprintf("%s  Thinking…", spin), th.text, th.panel)

	lines := []string{
		"Plan:",
		"  1) Parse drawlist bytes",
		"  2) Execute into framebuffer",
		"  3) Diff and single-flush",
		"",
		"Notes:",
		"  - Deterministic output",
		"  - Buffer-oriented ABI",
		"  - Platform boundary enforced",
	}
	for i := 0; i < len(lines); i++ {
		y := card.y + 5 + i
		if y >= card.y+card.h-2 {
			break
		}
		fg := th.muted
		if strings.HasSuffix(lines[i], ":") {
			fg = th.accent2
		}
		uiTextClamp(b, card.x+2, y, card.w-4, lines[i], fg, th.panel)
	}
}

type matrixState struct {
	rng xorshift32

	rowBuf []byte
}

func (s *matrixState) Reset() {
	s.rng.Seed(0xC0D3F001)
}

func (s *matrixState) Draw(b *dlBuilder, r rect, th theme) {
	uiFill(b, r, th.text, rgb(3, 6, 12))

	if r.w <= 0 || r.h <= 0 {
		return
	}

	if len(s.rowBuf) != r.w {
		s.rowBuf = make([]byte, r.w)
	}
	row := s.rowBuf
	for y := 0; y < r.h; y++ {
		for x := 0; x < r.w; x++ {
			v := s.rng.Next()
			if v%7 == 0 {
				row[x] = byte('A' + (v % 26))
			} else if v%19 == 0 {
				row[x] = byte('0' + (v % 10))
			} else {
				row[x] = ' '
			}
		}
		idx := b.AddStringBytes(row)
		b.CmdDrawTextSlice(int32(r.x), int32(r.y+y), idx, 0, uint32(len(row)), dlStyle{fg: rgb(34, 197, 94), bg: rgb(3, 6, 12)})
	}
}

type stormState struct {
	rng     xorshift32
	n       int
	charset string
}

func (s *stormState) Reset() {
	s.rng.Seed(0x5157F00D)
	if s.n == 0 {
		s.n = 75000
	}
	s.charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#$%&*+-=<>?"
}

func (s *stormState) Draw(b *dlBuilder, r rect, th theme) {
	uiFill(b, r, th.text, rgb(5, 8, 18))

	if r.w <= 0 || r.h <= 0 {
		return
	}
	idx := b.AddString(s.charset)

	fg := rgb(229, 231, 235)
	bg := rgb(5, 8, 18)

	for i := 0; i < s.n; i++ {
		v := s.rng.Next()
		x := r.x + int(v%uint32(r.w))
		y := r.y + int((v>>16)%uint32(r.h))
		chOff := uint32(v % uint32(len(s.charset)))
		b.CmdDrawTextSlice(int32(x), int32(y), idx, chOff, 1, dlStyle{fg: fg, bg: bg})
	}
}
