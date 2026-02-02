package main

import (
	"time"
)

type stormComet struct {
	x float32
	y float32

	vx float32
	vy float32

	colorIdx uint8
	phase    uint16
}

type particleStormState struct {
	rng   xorshift32
	frame uint32

	n          int
	visibleMax int

	cols int
	rows int

	comets []stormComet
}

func (s *particleStormState) SetCount(n int) {
	if n < 0 {
		n = 0
	}
	s.n = n
	if s.visibleMax == 0 {
		s.visibleMax = 20000
	}
}

func (s *particleStormState) SetVisibleMax(n int) {
	if n < 0 {
		n = 0
	}
	s.visibleMax = n
}

func (s *particleStormState) Reset(now time.Time) {
	seed := uint32(now.UnixNano()) ^ 0x5354524D // "STRM"
	if seed == 0 {
		seed = 1
	}
	s.rng.Seed(seed)
	s.frame = 0
	s.cols = 0
	s.rows = 0
	s.comets = s.comets[:0]
	if s.visibleMax == 0 {
		s.visibleMax = 20000
	}
}

func (s *particleStormState) ensureSize(w, h int) {
	if w <= 0 || h <= 0 {
		return
	}
	if s.cols == w && s.rows == h {
		return
	}
	s.cols = w
	s.rows = h
}

func stormBlend(bg, fg uint32, a uint8) uint32 {
	br := uint8(bg >> 16)
	bg2 := uint8(bg >> 8)
	bb := uint8(bg)

	fr := uint8(fg >> 16)
	fg2 := uint8(fg >> 8)
	fb := uint8(fg)

	ai := uint32(a)
	inv := uint32(255 - a)
	r := (uint32(br)*inv + uint32(fr)*ai) / 255
	g := (uint32(bg2)*inv + uint32(fg2)*ai) / 255
	b := (uint32(bb)*inv + uint32(fb)*ai) / 255
	return rgb(uint8(r), uint8(g), uint8(b))
}

func (s *particleStormState) ensureComets(visibleSegments int, trailLen int) {
	if visibleSegments <= 0 || trailLen <= 0 {
		s.comets = s.comets[:0]
		return
	}

	want := visibleSegments / trailLen
	if want < 8 {
		want = 8
	}
	if want > 6000 {
		want = 6000
	}

	if len(s.comets) == want {
		return
	}

	old := len(s.comets)
	if want < old {
		s.comets = s.comets[:want]
		return
	}

	s.comets = append(s.comets, make([]stormComet, want-old)...)
	for i := old; i < want; i++ {
		v := s.rng.Next()
		s.comets[i] = stormComet{
			x:        float32(v % uint32(maxI(1, s.cols))),
			y:        -float32((v >> 8) % uint32(maxI(1, s.rows)+trailLen+8)),
			vx:       (float32(int32((v>>20)&7))-3.0)*0.05 + 0.02,
			vy:       0.55 + float32((v>>12)%200)/140.0,
			colorIdx: uint8((v >> 28) % 4),
			phase:    uint16((v >> 4) % 4096),
		}
	}
}

func maxI(a, b int) int {
	if a > b {
		return a
	}
	return b
}

func (s *particleStormState) Draw(b *dlBuilder, r rect, th theme, now time.Time) {
	_ = now
	if r.w <= 0 || r.h <= 0 {
		return
	}

	s.ensureSize(r.w, r.h)
	s.frame++

	const trailLen = 18

	target := s.n
	if target < 0 {
		target = 0
	}
	visibleTarget := target
	if s.visibleMax < visibleTarget {
		visibleTarget = s.visibleMax
	}
	if visibleTarget < 0 {
		visibleTarget = 0
	}
	phantom := target - visibleTarget

	bg := rgb(0, 0, 0)
	uiFill(b, r, th.text, bg)

	palBase := [4]uint32{
		rgb(80, 250, 123),
		rgb(125, 211, 252),
		rgb(244, 114, 182),
		rgb(250, 204, 21),
	}
	pal := [4][4]uint32{}
	for i := 0; i < 4; i++ {
		pal[i][0] = stormBlend(bg, palBase[i], 60)
		pal[i][1] = stormBlend(bg, palBase[i], 110)
		pal[i][2] = stormBlend(bg, palBase[i], 170)
		pal[i][3] = stormBlend(bg, palBase[i], 240)
	}

	gTail := b.AddString("░")
	gMid := b.AddString("▒")
	gNear := b.AddString("▓")
	gHead := b.AddString("█")

	s.ensureComets(visibleTarget, trailLen)

	remain := visibleTarget
	stPhantom := dlStyle{fg: th.text, bg: bg, attrs: 0}

	for i := 0; i < len(s.comets); i++ {
		c := &s.comets[i]

		h := uint32(c.phase) + s.frame*747796405
		wob := float32(int32((h>>27)&31)-16) * 0.04
		c.y += c.vy
		c.x += c.vx + wob*0.06

		if c.y-float32(trailLen) > float32(s.rows+2) {
			v := s.rng.Next()
			c.x = float32(v % uint32(maxI(1, s.cols)))
			c.y = -float32((v >> 8) % uint32(trailLen+12))
			c.vx = (float32(int32((v>>20)&7))-3.0)*0.05 + 0.02
			c.vy = 0.60 + float32((v>>12)%220)/145.0
			c.colorIdx = uint8((v >> 28) % 4)
			c.phase = uint16((v >> 4) % 4096)
		}

		for k := 0; k < trailLen; k++ {
			if remain <= 0 {
				break
			}

			t := float32(k)
			x := int(c.x - t*(0.45+c.vx*2.0))
			y := int(c.y - t*(0.95-c.vx*1.2))

			level := 0
			glyph := gTail
			if k <= 1 {
				level = 3
				glyph = gHead
			} else if k <= 4 {
				level = 2
				glyph = gNear
			} else if k <= 9 {
				level = 1
				glyph = gMid
			}

			col := pal[c.colorIdx][level]
			if x < 0 || x >= s.cols || y < 0 || y >= s.rows {
				b.CmdFillRect(-1000000, -1000000, 1, 1, stPhantom)
			} else {
				st := dlStyle{fg: col, bg: bg, attrs: 0}
				b.CmdDrawTextSlice(int32(r.x+x), int32(r.y+y), glyph, 0, uint32(len("█")), st)
			}
			remain--
		}
	}

	for remain > 0 {
		b.CmdFillRect(-1000000, -1000000, 1, 1, stPhantom)
		remain--
	}

	for i := 0; i < phantom; i++ {
		b.CmdFillRect(-1000000, -1000000, 1, 1, stPhantom)
	}

	panel := rect{x: r.x + 2, y: r.y + 1, w: 36, h: 4}.clamp()
	if panel.w > 18 && panel.h > 2 {
		pbg := rgb(6, 10, 18)
		uiFill(b, panel, th.text, pbg)
		uiTextClamp(b, panel.x+2, panel.y+1, panel.w-4, "Neon Particle Storm", th.accent2, pbg)
		vis := visibleTarget
		if vis < 0 {
			vis = 0
		}
		ph := target - vis
		if ph < 0 {
			ph = 0
		}
		uiTextClamp(b, panel.x+2, panel.y+2, panel.w-4, "visible "+itoa(vis)+"  phantom "+itoa(ph), th.muted, pbg)
	}
}

func itoa(v int) string {
	if v == 0 {
		return "0"
	}
	neg := false
	if v < 0 {
		neg = true
		v = -v
	}
	var buf [32]byte
	i := len(buf)
	for v > 0 {
		i--
		buf[i] = byte('0' + (v % 10))
		v /= 10
	}
	if neg {
		i--
		buf[i] = '-'
	}
	return string(buf[i:])
}
