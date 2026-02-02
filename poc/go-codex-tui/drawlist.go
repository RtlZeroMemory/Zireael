package main

/*
  poc/go-codex-tui/drawlist.go — Drawlist v1 builder for the Go demo.

  Why: Zireael expects a versioned, little-endian drawlist byte stream. This
  builder emits a minimal v1-compliant drawlist with a command section and a
  string table section (no blobs in this demo).
*/

import (
	"encoding/binary"
)

const (
	zrDlMagic = uint32(0x4C44525A) // 'ZRDL' little-endian u32

	zrDlVersionV1 = uint32(1)

	zrDlOpClear       = uint16(1)
	zrDlOpFillRect    = uint16(2)
	zrDlOpDrawText    = uint16(3)
	zrDlOpPushClip    = uint16(4)
	zrDlOpPopClip     = uint16(5)
	zrDlOpDrawTextRun = uint16(6)
)

type dlStyle struct {
	fg    uint32
	bg    uint32
	attrs uint32
}

type dlSpan struct {
	off uint32
	len uint32
}

type dlBuilder struct {
	cmd      []byte
	cmdCount uint32

	stringsSpans []dlSpan
	stringsBytes []byte

	blobsSpans []dlSpan
	blobsBytes []byte

	out []byte
}

func (b *dlBuilder) Reset() {
	b.cmd = b.cmd[:0]
	b.cmdCount = 0
	b.stringsSpans = b.stringsSpans[:0]
	b.stringsBytes = b.stringsBytes[:0]
	b.blobsSpans = b.blobsSpans[:0]
	b.blobsBytes = b.blobsBytes[:0]
}

func (b *dlBuilder) Reserve(cmdBytesCap int, stringsBytesCap int, stringsCap int) {
	if cap(b.cmd) < cmdBytesCap {
		b.cmd = make([]byte, 0, cmdBytesCap)
	}
	if cap(b.stringsBytes) < stringsBytesCap {
		b.stringsBytes = make([]byte, 0, stringsBytesCap)
	}
	if cap(b.stringsSpans) < stringsCap {
		b.stringsSpans = make([]dlSpan, 0, stringsCap)
	}
}

func dlAlign4(v uint32) uint32 { return (v + 3) &^ 3 }

func putU16LE(p []byte, v uint16) {
	p[0] = byte(v)
	p[1] = byte(v >> 8)
}

func putU32LE(p []byte, v uint32) {
	p[0] = byte(v)
	p[1] = byte(v >> 8)
	p[2] = byte(v >> 16)
	p[3] = byte(v >> 24)
}

func (b *dlBuilder) ReserveBlobs(bytesCap int, blobsCap int) {
	if cap(b.blobsBytes) < bytesCap {
		b.blobsBytes = make([]byte, 0, bytesCap)
	}
	if cap(b.blobsSpans) < blobsCap {
		b.blobsSpans = make([]dlSpan, 0, blobsCap)
	}
}

func (b *dlBuilder) appendTo(buf []byte, n int) ([]byte, []byte) {
	if n <= 0 {
		return buf, nil
	}
	old := len(buf)
	need := old + n
	if need > cap(buf) {
		newCap := cap(buf) * 2
		if newCap < need {
			newCap = need
		}
		tmp := make([]byte, old, newCap)
		copy(tmp, buf)
		buf = tmp
	}
	buf = buf[:need]
	return buf, buf[old:need]
}

func (b *dlBuilder) appendCmdBytes(n int) []byte {
	var out []byte
	b.cmd, out = b.appendTo(b.cmd, n)
	return out
}

func (b *dlBuilder) appendBlobBytes(n int) []byte {
	var out []byte
	b.blobsBytes, out = b.appendTo(b.blobsBytes, n)
	return out
}

func (b *dlBuilder) AddStringBytes(s []byte) uint32 {
	off := uint32(len(b.stringsBytes))
	b.stringsBytes = append(b.stringsBytes, s...)
	b.stringsSpans = append(b.stringsSpans, dlSpan{off: off, len: uint32(len(s))})
	return uint32(len(b.stringsSpans) - 1)
}

func (b *dlBuilder) AddString(s string) uint32 { return b.AddStringBytes([]byte(s)) }

type dlTextRunSeg struct {
	style dlStyle

	stringIndex uint32
	byteOff     uint32
	byteLen     uint32
}

func (b *dlBuilder) AddTextRunBlob(segs []dlTextRunSeg) uint32 {
	off := uint32(len(b.blobsBytes))
	aligned := dlAlign4(off)
	if aligned != off {
		pad := int(aligned - off)
		p := b.appendBlobBytes(pad)
		for i := range p {
			p[i] = 0
		}
		off = aligned
	}

	const segSize = 28
	blobLen := 4 + len(segs)*segSize
	if blobLen == 0 {
		blobLen = 4
	}

	p := b.appendBlobBytes(blobLen)
	putU32LE(p[0:], uint32(len(segs)))
	wp := 4
	for i := 0; i < len(segs); i++ {
		s := segs[i]
		putU32LE(p[wp+0:], s.style.fg)
		putU32LE(p[wp+4:], s.style.bg)
		putU32LE(p[wp+8:], s.style.attrs)
		putU32LE(p[wp+12:], 0)
		putU32LE(p[wp+16:], s.stringIndex)
		putU32LE(p[wp+20:], s.byteOff)
		putU32LE(p[wp+24:], s.byteLen)
		wp += segSize
	}

	b.blobsSpans = append(b.blobsSpans, dlSpan{off: off, len: uint32(blobLen)})
	return uint32(len(b.blobsSpans) - 1)
}

func (b *dlBuilder) CmdClear() {
	const cmdSize = 8
	p := b.appendCmdBytes(cmdSize)
	putU16LE(p[0:], zrDlOpClear)
	putU16LE(p[2:], 0)
	putU32LE(p[4:], uint32(cmdSize))
	b.cmdCount++
}

func (b *dlBuilder) CmdPushClip(x, y, w, h int32) {
	const cmdSize = 8 + 16
	p := b.appendCmdBytes(cmdSize)
	putU16LE(p[0:], zrDlOpPushClip)
	putU16LE(p[2:], 0)
	putU32LE(p[4:], uint32(cmdSize))
	putU32LE(p[8:], uint32(x))
	putU32LE(p[12:], uint32(y))
	putU32LE(p[16:], uint32(w))
	putU32LE(p[20:], uint32(h))
	b.cmdCount++
}

func (b *dlBuilder) CmdPopClip() {
	const cmdSize = 8
	p := b.appendCmdBytes(cmdSize)
	putU16LE(p[0:], zrDlOpPopClip)
	putU16LE(p[2:], 0)
	putU32LE(p[4:], uint32(cmdSize))
	b.cmdCount++
}

func (b *dlBuilder) CmdFillRect(x, y, w, h int32, st dlStyle) {
	const cmdSize = 8 + 32
	p := b.appendCmdBytes(cmdSize)
	putU16LE(p[0:], zrDlOpFillRect)
	putU16LE(p[2:], 0)
	putU32LE(p[4:], uint32(cmdSize))

	putU32LE(p[8:], uint32(x))
	putU32LE(p[12:], uint32(y))
	putU32LE(p[16:], uint32(w))
	putU32LE(p[20:], uint32(h))

	putU32LE(p[24:], st.fg)
	putU32LE(p[28:], st.bg)
	putU32LE(p[32:], st.attrs)
	putU32LE(p[36:], 0) // reserved0
	b.cmdCount++
}

func (b *dlBuilder) CmdDrawTextSlice(x, y int32, stringIndex uint32, byteOff, byteLen uint32, st dlStyle) {
	const cmdSize = 8 + 40
	p := b.appendCmdBytes(cmdSize)
	putU16LE(p[0:], zrDlOpDrawText)
	putU16LE(p[2:], 0)
	putU32LE(p[4:], uint32(cmdSize))

	putU32LE(p[8:], uint32(x))
	putU32LE(p[12:], uint32(y))
	putU32LE(p[16:], stringIndex)
	putU32LE(p[20:], byteOff)
	putU32LE(p[24:], byteLen)

	putU32LE(p[28:], st.fg)
	putU32LE(p[32:], st.bg)
	putU32LE(p[36:], st.attrs)
	putU32LE(p[40:], 0) // style.reserved0
	putU32LE(p[44:], 0) // cmd.reserved0
	b.cmdCount++
}

func (b *dlBuilder) CmdDrawText(x, y int32, s string, st dlStyle) {
	idx := b.AddString(s)
	b.CmdDrawTextSlice(x, y, idx, 0, uint32(len(s)), st)
}

func (b *dlBuilder) CmdDrawTextRun(x, y int32, blobIndex uint32) {
	const cmdSize = 8 + 16
	p := b.appendCmdBytes(cmdSize)
	putU16LE(p[0:], zrDlOpDrawTextRun)
	putU16LE(p[2:], 0)
	putU32LE(p[4:], uint32(cmdSize))

	putU32LE(p[8:], uint32(x))
	putU32LE(p[12:], uint32(y))
	putU32LE(p[16:], blobIndex)
	putU32LE(p[20:], 0)
	b.cmdCount++
}

func (b *dlBuilder) Build() []byte {
	/*
		Drawlist v1 layout:
		  [header][cmd bytes][string spans][string bytes][blob spans][blob bytes]
	*/
	const headerSize = 64

	cmdOff := uint32(headerSize)
	cmdBytes := uint32(len(b.cmd))

	stringsCount := uint32(len(b.stringsSpans))
	stringsSpanBytes := stringsCount * 8
	stringsBytesLenRaw := uint32(len(b.stringsBytes))
	stringsBytesLen := dlAlign4(stringsBytesLenRaw)

	blobsCount := uint32(len(b.blobsSpans))
	blobsSpanBytes := blobsCount * 8
	blobsBytesLenRaw := uint32(len(b.blobsBytes))
	blobsBytesLen := dlAlign4(blobsBytesLenRaw)

	stringsSpanOff := uint32(0)
	stringsBytesOff := uint32(0)
	if stringsCount != 0 {
		stringsSpanOff = dlAlign4(cmdOff + cmdBytes)
		stringsBytesOff = dlAlign4(stringsSpanOff + stringsSpanBytes)
	}

	blobsSpanOff := uint32(0)
	blobsBytesOff := uint32(0)
	if blobsCount != 0 {
		blobsSpanOff = dlAlign4(stringsBytesOff + stringsBytesLen)
		blobsBytesOff = dlAlign4(blobsSpanOff + blobsSpanBytes)
	}

	totalSize := stringsBytesOff + stringsBytesLen
	if blobsCount != 0 {
		totalSize = blobsBytesOff + blobsBytesLen
	}

	if cap(b.out) < int(totalSize) {
		b.out = make([]byte, totalSize)
	} else {
		b.out = b.out[:totalSize]
	}

	h := b.out[:headerSize]
	binary.LittleEndian.PutUint32(h[0:], zrDlMagic)
	binary.LittleEndian.PutUint32(h[4:], zrDlVersionV1)
	binary.LittleEndian.PutUint32(h[8:], uint32(headerSize))
	binary.LittleEndian.PutUint32(h[12:], totalSize)
	binary.LittleEndian.PutUint32(h[16:], cmdOff)
	binary.LittleEndian.PutUint32(h[20:], cmdBytes)
	binary.LittleEndian.PutUint32(h[24:], b.cmdCount)
	binary.LittleEndian.PutUint32(h[28:], stringsSpanOff)
	binary.LittleEndian.PutUint32(h[32:], stringsCount)
	binary.LittleEndian.PutUint32(h[36:], stringsBytesOff)
	binary.LittleEndian.PutUint32(h[40:], stringsBytesLen)
	binary.LittleEndian.PutUint32(h[44:], blobsSpanOff)
	binary.LittleEndian.PutUint32(h[48:], blobsCount)
	binary.LittleEndian.PutUint32(h[52:], blobsBytesOff)
	binary.LittleEndian.PutUint32(h[56:], blobsBytesLen)
	binary.LittleEndian.PutUint32(h[60:], 0) // reserved0

	copy(b.out[cmdOff:cmdOff+cmdBytes], b.cmd)

	if stringsCount != 0 {
		spanBase := b.out[stringsSpanOff : stringsSpanOff+stringsSpanBytes]
		for i, sp := range b.stringsSpans {
			p := spanBase[i*8 : i*8+8]
			binary.LittleEndian.PutUint32(p[0:], sp.off)
			binary.LittleEndian.PutUint32(p[4:], sp.len)
		}

		copy(b.out[stringsBytesOff:stringsBytesOff+stringsBytesLenRaw], b.stringsBytes)
		for i := stringsBytesOff + stringsBytesLenRaw; i < stringsBytesOff+stringsBytesLen; i++ {
			b.out[i] = 0
		}
	}

	if blobsCount != 0 {
		bspanBase := b.out[blobsSpanOff : blobsSpanOff+blobsSpanBytes]
		for i, sp := range b.blobsSpans {
			p := bspanBase[i*8 : i*8+8]
			binary.LittleEndian.PutUint32(p[0:], sp.off)
			binary.LittleEndian.PutUint32(p[4:], sp.len)
		}

		copy(b.out[blobsBytesOff:blobsBytesOff+blobsBytesLenRaw], b.blobsBytes)
		for i := blobsBytesOff + blobsBytesLenRaw; i < blobsBytesOff+blobsBytesLen; i++ {
			b.out[i] = 0
		}
	}
	return b.out
}
