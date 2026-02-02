package main

import (
	"encoding/binary"
	"fmt"
)

const (
	zrEvMagic = uint32(0x5645525A) // 'ZREV' little-endian u32

	zrEventBatchVersionV1 = uint32(1)

	zrEvKey    = uint32(1)
	zrEvText   = uint32(2)
	zrEvPaste  = uint32(3)
	zrEvMouse  = uint32(4)
	zrEvResize = uint32(5)
	zrEvTick   = uint32(6)
	zrEvUser   = uint32(7)
)

const (
	zrKeyUnknown   = uint32(0)
	zrKeyEscape    = uint32(1)
	zrKeyEnter     = uint32(2)
	zrKeyTab       = uint32(3)
	zrKeyBackspace = uint32(4)

	zrKeyUp    = uint32(20)
	zrKeyDown  = uint32(21)
	zrKeyLeft  = uint32(22)
	zrKeyRight = uint32(23)
)

const (
	zrKeyActionDown   = uint32(1)
	zrKeyActionUp     = uint32(2)
	zrKeyActionRepeat = uint32(3)
)

type appEventKind uint8

const (
	appEventInvalid appEventKind = iota
	appEventKey
	appEventText
	appEventResize
)

type appEvent struct {
	kind appEventKind

	keyKey    uint32
	keyAction uint32

	textRune rune

	resizeCols int
	resizeRows int
}

func parseEventBatch(buf []byte) ([]appEvent, error) {
	if len(buf) == 0 {
		return nil, nil
	}
	if len(buf) < 24 {
		return nil, fmt.Errorf("event batch too small: %d", len(buf))
	}

	magic := binary.LittleEndian.Uint32(buf[0:])
	if magic != zrEvMagic {
		return nil, fmt.Errorf("bad event magic: 0x%08x", magic)
	}
	ver := binary.LittleEndian.Uint32(buf[4:])
	if ver != zrEventBatchVersionV1 {
		return nil, fmt.Errorf("unsupported event batch version: %d", ver)
	}
	totalSize := binary.LittleEndian.Uint32(buf[8:])
	if totalSize > uint32(len(buf)) {
		return nil, fmt.Errorf("event batch total_size %d > buf %d", totalSize, len(buf))
	}
	if totalSize < 24 {
		return nil, fmt.Errorf("event batch total_size too small: %d", totalSize)
	}

	var out []appEvent
	off := 24
	end := int(totalSize)

	for off < end {
		if end-off < 16 {
			return nil, fmt.Errorf("truncated record header at %d", off)
		}
		typ := binary.LittleEndian.Uint32(buf[off+0:])
		sz := int(binary.LittleEndian.Uint32(buf[off+4:]))
		if sz < 16 {
			return nil, fmt.Errorf("record size too small: %d", sz)
		}
		if off+sz > end {
			return nil, fmt.Errorf("record overruns batch: off=%d size=%d end=%d", off, sz, end)
		}

		payload := buf[off+16 : off+sz]

		switch typ {
		case zrEvKey:
			if len(payload) < 16 {
				return nil, fmt.Errorf("key payload too small: %d", len(payload))
			}
			key := binary.LittleEndian.Uint32(payload[0:])
			action := binary.LittleEndian.Uint32(payload[8:])
			out = append(out, appEvent{
				kind:      appEventKey,
				keyKey:    key,
				keyAction: action,
			})
		case zrEvText:
			if len(payload) < 8 {
				return nil, fmt.Errorf("text payload too small: %d", len(payload))
			}
			cp := binary.LittleEndian.Uint32(payload[0:])
			out = append(out, appEvent{
				kind:     appEventText,
				textRune: rune(cp),
			})
		case zrEvResize:
			if len(payload) < 16 {
				return nil, fmt.Errorf("resize payload too small: %d", len(payload))
			}
			cols := int(binary.LittleEndian.Uint32(payload[0:]))
			rows := int(binary.LittleEndian.Uint32(payload[4:]))
			out = append(out, appEvent{
				kind:       appEventResize,
				resizeCols: cols,
				resizeRows: rows,
			})
		default:
			// Forward-compat: unknown types are skippable by size.
		}

		off += sz
	}
	return out, nil
}
