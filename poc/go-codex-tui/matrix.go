//go:build !windows

package main

import (
	"time"
)

type matrixCol struct {
	head   int
	speed  int
	trail  int
	seed   uint32
	phase  int
	jitter int
}

type matrixRainState struct {
	rng   xorshift32
	frame uint32

	cols int
	rows int
	col  []matrixCol

	charset [][]byte
	rowBuf  []byte

	segBounds []matrixSegBound
	runSegs   []dlTextRunSeg
}

type matrixSegBound struct {
	cat   int
	start int
	end   int
}

func (s *matrixRainState) Reset() {
	s.rng.Seed(0x4D415452) // "MATR"
	s.frame = 0
	s.charset = s.charset[:0]
	s.rowBuf = s.rowBuf[:0]
	s.cols = 0
	s.rows = 0
	s.col = s.col[:0]

	chars := []rune(" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")
	half := []rune("ｦｧｨｩｪｫｬｭｮｯｰｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ")
	chars = append(chars, half...)

	for _, r := range chars {
		s.charset = append(s.charset, []byte(string(r)))
	}
}

func (s *matrixRainState) ensureSize(w, h int) {
	if w <= 0 || h <= 0 {
		return
	}
	if s.cols == w && s.rows == h && len(s.col) == w {
		return
	}
	s.cols = w
	s.rows = h
	s.col = make([]matrixCol, w)

	for x := 0; x < w; x++ {
		v := s.rng.Next()
		speed := 1 + int(v%3)
		trail := 6 + int((v>>8)%22)
		head := -int((v >> 16) % uint32(h+trail+8))
		s.col[x] = matrixCol{
			head:   head,
			speed:  speed,
			trail:  trail,
			seed:   v ^ (uint32(x) * 0x9E3779B9),
			phase:  int((v >> 24) % 17),
			jitter: int((v >> 12) % 5),
		}
	}
}

func (s *matrixRainState) Draw(b *dlBuilder, r rect, th theme, now time.Time) {
	bg := rgb(0, 0, 0)
	fgDim := rgb(10, 80, 40)
	uiFill(b, r, fgDim, bg)
	if r.w <= 0 || r.h <= 0 {
		return
	}

	s.ensureSize(r.w, r.h)
	s.frame++

	for x := 0; x < s.cols; x++ {
		c := &s.col[x]
		if int(s.frame)%c.speed == 0 {
			c.head++
		}
		if c.head > s.rows+c.trail+2 {
			v := s.rng.Next()
			c.speed = 1 + int(v%3)
			c.trail = 6 + int((v>>8)%26)
			c.head = -int((v >> 16) % uint32(s.rows+c.trail+8))
			c.seed ^= v
			c.phase = int((v >> 24) % 19)
			c.jitter = int((v >> 12) % 7)
		}
	}

	fgMid := rgb(34, 197, 94)
	fgHot := rgb(190, 255, 220)
	fgSpark := rgb(56, 189, 248)

	for y := 0; y < s.rows; y++ {
		s.rowBuf = s.rowBuf[:0]

		s.segBounds = s.segBounds[:0]
		segCat := -1
		segStart := 0

		for x := 0; x < s.cols; x++ {
			c := s.col[x]
			d := c.head - y
			cat := 0
			if d == 0 {
				cat = 3
			} else if d > 0 && d <= c.trail {
				if d <= 2 {
					cat = 2
				} else {
					cat = 1
				}
				if (int(s.frame)+x+y+c.phase)%97 == 0 {
					cat = 2
				}
			}

			var glyph []byte
			if cat == 0 {
				glyph = nil
			} else {
				h := uint32(y*1315423911) ^ uint32(x*2654435761) ^ (s.frame * 374761393) ^ c.seed
				idx := int(h % uint32(len(s.charset)))
				glyph = s.charset[idx]
			}

			bytePos := len(s.rowBuf)
			if segCat == -1 {
				segCat = cat
				segStart = bytePos
			} else if cat != segCat {
				s.segBounds = append(s.segBounds, matrixSegBound{cat: segCat, start: segStart, end: bytePos})
				segCat = cat
				segStart = bytePos
			}

			if cat == 0 {
				s.rowBuf = append(s.rowBuf, byte(' '))
			} else {
				s.rowBuf = append(s.rowBuf, glyph...)
			}
		}
		if segCat != -1 {
			s.segBounds = append(s.segBounds, matrixSegBound{cat: segCat, start: segStart, end: len(s.rowBuf)})
		}

		rowIdx := b.AddStringBytes(s.rowBuf)
		s.runSegs = s.runSegs[:0]
		for i := 0; i < len(s.segBounds); i++ {
			sb := s.segBounds[i]
			fg := fgDim
			if sb.cat == 2 {
				fg = fgMid
			} else if sb.cat == 3 {
				fg = fgHot
			}
			if sb.cat != 0 && ((int(s.frame)+i+y)%211 == 0) {
				fg = fgSpark
			}
			s.runSegs = append(s.runSegs, dlTextRunSeg{
				style:       dlStyle{fg: fg, bg: bg, attrs: 0},
				stringIndex: rowIdx,
				byteOff:     uint32(sb.start),
				byteLen:     uint32(sb.end - sb.start),
			})
		}

		blob := b.AddTextRunBlob(s.runSegs)
		b.CmdDrawTextRun(int32(r.x), int32(r.y+y), blob)
	}

	panel := rect{x: r.x + 2, y: r.y + 1, w: 28, h: 3}.clamp()
	if panel.w > 12 && panel.h > 2 {
		pbg := rgb(6, 10, 18)
		uiFill(b, panel, th.text, pbg)
		uiTextClamp(b, panel.x+2, panel.y+1, panel.w-4, "Matrix Rain", fgMid, pbg)
	}
}
