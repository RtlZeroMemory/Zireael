//go:build !windows

package main

import "time"

type particle struct {
	x uint16
	y uint16
	vx int8
	vy int8
	c  uint8
}

type particleStormState struct {
	rng xorshift32

	w int
	h int

	n int
	visibleMax int

	parts   []particle
	palette []uint32

	fillCellTmpl [40]byte
}

func (s *particleStormState) Reset(now time.Time) {
	_ = now
	s.rng.Seed(0x53544F52) // "STOR"
	if s.n == 0 {
		s.n = 60000
	}
	if s.visibleMax == 0 {
		s.visibleMax = 20000
	}
	s.palette = make([]uint32, 0, 64)
	s.initPalette()
	s.parts = nil
	s.w = 0
	s.h = 0
}

func (s *particleStormState) SetCount(n int) {
	if n < 0 {
		n = 0
	}
	s.n = n
}

func (s *particleStormState) SetVisibleMax(n int) {
	if n < 0 {
		n = 0
	}
	s.visibleMax = n
}

func (s *particleStormState) visibleCount() int {
	if s.n < s.visibleMax {
		return s.n
	}
	return s.visibleMax
}

func (s *particleStormState) initPalette() {
	// A compact neon palette (RGB), tuned for terminal color mapping.
	base := []uint32{
		rgb(94, 234, 212),  // teal
		rgb(129, 140, 248), // indigo
		rgb(250, 204, 21),  // amber
		rgb(244, 114, 182), // pink
		rgb(74, 222, 128),  // green
		rgb(56, 189, 248),  // cyan
		rgb(251, 113, 133), // rose
		rgb(232, 121, 249), // fuchsia
	}
	for i := 0; i < 8; i++ {
		s.palette = append(s.palette, base...)
	}
}

func (s *particleStormState) ensure(w, h int) {
	if w <= 0 || h <= 0 {
		return
	}
	want := s.visibleCount()
	if s.w == w && s.h == h && len(s.parts) == want {
		return
	}
	s.w = w
	s.h = h
	s.parts = make([]particle, want)

	for i := 0; i < len(s.parts); i++ {
		v := s.rng.Next()
		x := uint16(v % uint32(w))
		y := uint16((v >> 16) % uint32(h))
		vx := int8((v&3) - 1) // -1..2
		vy := int8(((v>>2)&3) - 1)
		if vx == 0 && vy == 0 {
			vx = 1
		}
		c := uint8((v >> 8) % uint32(len(s.palette)))
		s.parts[i] = particle{x: x, y: y, vx: vx, vy: vy, c: c}
	}
}

func (s *particleStormState) Draw(b *dlBuilder, r rect, th theme, now time.Time) {
	_ = now
	bgTop := rgb(6, 10, 22)
	bgBot := rgb(10, 8, 26)
	if r.w <= 0 || r.h <= 0 {
		return
	}

	s.ensure(r.w, r.h)

	// Subtle split-tone background for a "nicer" look than a flat fill.
	top := rect{x: r.x, y: r.y, w: r.w, h: r.h / 2}.clamp()
	bot := rect{x: r.x, y: r.y + r.h/2, w: r.w, h: r.h - r.h/2}.clamp()
	uiFill(b, top, th.text, bgTop)
	uiFill(b, bot, th.text, bgBot)

	w := uint16(r.w)
	h := uint16(r.h)

	/* Build a fixed fill-cell command template and patch only x/y/bg per particle. */
	{
		p := s.fillCellTmpl[:]
		putU16LE(p[0:], zrDlOpFillRect)
		putU16LE(p[2:], 0)
		putU32LE(p[4:], 40)
		putU32LE(p[8:], 0)
		putU32LE(p[12:], 0)
		putU32LE(p[16:], 1)
		putU32LE(p[20:], 1)
		putU32LE(p[24:], th.text)
		putU32LE(p[28:], 0)
		putU32LE(p[32:], 0)
		putU32LE(p[36:], 0)
	}

	for i := 0; i < len(s.parts); i++ {
		p := &s.parts[i]

		x := int(p.x) + int(p.vx)
		y := int(p.y) + int(p.vy)
		if x < 0 {
			x += int(w)
		} else if x >= int(w) {
			x -= int(w)
		}
		if y < 0 {
			y += int(h)
		} else if y >= int(h) {
			y -= int(h)
		}

		p.x = uint16(x)
		p.y = uint16(y)

		c := s.palette[int(p.c)%len(s.palette)]
		cmd := b.appendCmdBytes(40)
		copy(cmd, s.fillCellTmpl[:])
		putU32LE(cmd[8:], uint32(r.x+x))
		putU32LE(cmd[12:], uint32(r.y+y))
		putU32LE(cmd[28:], c)
		b.cmdCount++
	}

	phantom := s.n - len(s.parts)
	if phantom > 0 {
		neg := int32(-1000000)
		px := uint32(neg)
		py := uint32(neg)
		for i := 0; i < phantom; i++ {
			cmd := b.appendCmdBytes(40)
			copy(cmd, s.fillCellTmpl[:])
			putU32LE(cmd[8:], px)
			putU32LE(cmd[12:], py)
			putU32LE(cmd[28:], bgTop)
			b.cmdCount++
		}
	}

	panel := rect{x: r.x + 2, y: r.y + 1, w: 36, h: 4}.clamp()
	if panel.w > 14 && panel.h >= 3 {
		pbg := rgb(14, 20, 40)
		uiFill(b, panel, th.text, pbg)
		uiTextClamp(b, panel.x+2, panel.y+1, panel.w-4, "Neon Particle Storm", th.accent, pbg)
		uiTextClamp(b, panel.x+2, panel.y+2, panel.w-4, "visible [ ]   total -storm-n   Q quit", th.muted, pbg)
	}
}
