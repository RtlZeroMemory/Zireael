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

	zrDrawlistVersionV1 = uint32(1)

	zrDlOpClear       = uint16(1)
	zrDlOpFillRect    = uint16(2)
	zrDlOpDrawText    = uint16(3)
	zrDlOpPushClip    = uint16(4)
	zrDlOpPopClip     = uint16(5)
	zrDlOpDrawTextRun = uint16(6)
)

type dlStyle struct {
	fg uint32
	bg uint32
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

	out []byte
}

func (b *dlBuilder) Reset() {
	b.cmd = b.cmd[:0]
	b.cmdCount = 0
	b.stringsSpans = b.stringsSpans[:0]
	b.stringsBytes = b.stringsBytes[:0]
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

func (b *dlBuilder) AddStringBytes(s []byte) uint32 {
	off := uint32(len(b.stringsBytes))
	b.stringsBytes = append(b.stringsBytes, s...)
	b.stringsSpans = append(b.stringsSpans, dlSpan{off: off, len: uint32(len(s))})
	return uint32(len(b.stringsSpans) - 1)
}

func (b *dlBuilder) AddString(s string) uint32 { return b.AddStringBytes([]byte(s)) }

func (b *dlBuilder) CmdClear() {
	const cmdSize = 8
	b.cmd = append(b.cmd, make([]byte, cmdSize)...)
	p := b.cmd[len(b.cmd)-cmdSize:]
	binary.LittleEndian.PutUint16(p[0:], zrDlOpClear)
	binary.LittleEndian.PutUint16(p[2:], 0)
	binary.LittleEndian.PutUint32(p[4:], uint32(cmdSize))
	b.cmdCount++
}

func (b *dlBuilder) CmdPushClip(x, y, w, h int32) {
	const cmdSize = 8 + 16
	b.cmd = append(b.cmd, make([]byte, cmdSize)...)
	p := b.cmd[len(b.cmd)-cmdSize:]
	binary.LittleEndian.PutUint16(p[0:], zrDlOpPushClip)
	binary.LittleEndian.PutUint16(p[2:], 0)
	binary.LittleEndian.PutUint32(p[4:], uint32(cmdSize))
	binary.LittleEndian.PutUint32(p[8:], uint32(x))
	binary.LittleEndian.PutUint32(p[12:], uint32(y))
	binary.LittleEndian.PutUint32(p[16:], uint32(w))
	binary.LittleEndian.PutUint32(p[20:], uint32(h))
	b.cmdCount++
}

func (b *dlBuilder) CmdPopClip() {
	const cmdSize = 8
	b.cmd = append(b.cmd, make([]byte, cmdSize)...)
	p := b.cmd[len(b.cmd)-cmdSize:]
	binary.LittleEndian.PutUint16(p[0:], zrDlOpPopClip)
	binary.LittleEndian.PutUint16(p[2:], 0)
	binary.LittleEndian.PutUint32(p[4:], uint32(cmdSize))
	b.cmdCount++
}

func (b *dlBuilder) CmdFillRect(x, y, w, h int32, st dlStyle) {
	const cmdSize = 8 + 32
	b.cmd = append(b.cmd, make([]byte, cmdSize)...)
	p := b.cmd[len(b.cmd)-cmdSize:]
	binary.LittleEndian.PutUint16(p[0:], zrDlOpFillRect)
	binary.LittleEndian.PutUint16(p[2:], 0)
	binary.LittleEndian.PutUint32(p[4:], uint32(cmdSize))

	binary.LittleEndian.PutUint32(p[8:], uint32(x))
	binary.LittleEndian.PutUint32(p[12:], uint32(y))
	binary.LittleEndian.PutUint32(p[16:], uint32(w))
	binary.LittleEndian.PutUint32(p[20:], uint32(h))

	binary.LittleEndian.PutUint32(p[24:], st.fg)
	binary.LittleEndian.PutUint32(p[28:], st.bg)
	binary.LittleEndian.PutUint32(p[32:], 0) // attrs
	binary.LittleEndian.PutUint32(p[36:], 0) // reserved0
	b.cmdCount++
}

func (b *dlBuilder) CmdDrawTextSlice(x, y int32, stringIndex uint32, byteOff, byteLen uint32, st dlStyle) {
	const cmdSize = 8 + 40
	b.cmd = append(b.cmd, make([]byte, cmdSize)...)
	p := b.cmd[len(b.cmd)-cmdSize:]
	binary.LittleEndian.PutUint16(p[0:], zrDlOpDrawText)
	binary.LittleEndian.PutUint16(p[2:], 0)
	binary.LittleEndian.PutUint32(p[4:], uint32(cmdSize))

	binary.LittleEndian.PutUint32(p[8:], uint32(x))
	binary.LittleEndian.PutUint32(p[12:], uint32(y))
	binary.LittleEndian.PutUint32(p[16:], stringIndex)
	binary.LittleEndian.PutUint32(p[20:], byteOff)
	binary.LittleEndian.PutUint32(p[24:], byteLen)

	binary.LittleEndian.PutUint32(p[28:], st.fg)
	binary.LittleEndian.PutUint32(p[32:], st.bg)
	binary.LittleEndian.PutUint32(p[36:], 0) // attrs
	binary.LittleEndian.PutUint32(p[40:], 0) // style.reserved0
	binary.LittleEndian.PutUint32(p[44:], 0) // cmd.reserved0
	b.cmdCount++
}

func (b *dlBuilder) CmdDrawText(x, y int32, s string, st dlStyle) {
	idx := b.AddString(s)
	b.CmdDrawTextSlice(x, y, idx, 0, uint32(len(s)), st)
}

func (b *dlBuilder) Build() []byte {
	/*
		Drawlist v1 layout:
		  [header][cmd bytes][string spans][string bytes][(no blobs)]
	*/
	const headerSize = 64

	cmdOff := uint32(headerSize)
	cmdBytes := uint32(len(b.cmd))

	stringsSpanOff := dlAlign4(cmdOff + cmdBytes)
	stringsCount := uint32(len(b.stringsSpans))
	stringsSpanBytes := stringsCount * 8

	stringsBytesOff := dlAlign4(stringsSpanOff + stringsSpanBytes)
	stringsBytesLenRaw := uint32(len(b.stringsBytes))
	stringsBytesLen := dlAlign4(stringsBytesLenRaw)

	totalSize := stringsBytesOff + stringsBytesLen

	if cap(b.out) < int(totalSize) {
		b.out = make([]byte, totalSize)
	} else {
		b.out = b.out[:totalSize]
	}

	h := b.out[:headerSize]
	binary.LittleEndian.PutUint32(h[0:], zrDlMagic)
	binary.LittleEndian.PutUint32(h[4:], zrDrawlistVersionV1)
	binary.LittleEndian.PutUint32(h[8:], uint32(headerSize))
	binary.LittleEndian.PutUint32(h[12:], totalSize)
	binary.LittleEndian.PutUint32(h[16:], cmdOff)
	binary.LittleEndian.PutUint32(h[20:], cmdBytes)
	binary.LittleEndian.PutUint32(h[24:], b.cmdCount)
	binary.LittleEndian.PutUint32(h[28:], stringsSpanOff)
	binary.LittleEndian.PutUint32(h[32:], stringsCount)
	binary.LittleEndian.PutUint32(h[36:], stringsBytesOff)
	binary.LittleEndian.PutUint32(h[40:], stringsBytesLen)
	binary.LittleEndian.PutUint32(h[44:], 0) // blobs_span_offset
	binary.LittleEndian.PutUint32(h[48:], 0) // blobs_count
	binary.LittleEndian.PutUint32(h[52:], 0) // blobs_bytes_offset
	binary.LittleEndian.PutUint32(h[56:], 0) // blobs_bytes_len
	binary.LittleEndian.PutUint32(h[60:], 0) // reserved0

	copy(b.out[cmdOff:cmdOff+cmdBytes], b.cmd)

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
	return b.out
}
