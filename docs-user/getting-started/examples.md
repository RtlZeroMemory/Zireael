# Examples

Complete examples showing how to use Zireael from C and Go.

## C: Basic Event Loop

```c
#include <zr/zr_engine.h>
#include <zr/zr_event.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    // Configure engine
    zr_engine_config_t cfg = zr_engine_config_default();
    cfg.requested_engine_abi_major = 1;
    cfg.requested_engine_abi_minor = 0;
    cfg.requested_drawlist_version = 1;
    cfg.requested_event_batch_version = 1;
    cfg.plat.mouse = 1;  // Enable mouse

    // Create engine (takes over terminal)
    zr_engine_t* engine = NULL;
    zr_result_t r = engine_create(&engine, &cfg);
    if (r != ZR_OK) {
        fprintf(stderr, "engine_create failed: %d\n", r);
        return 1;
    }

    // Event buffer
    uint8_t event_buf[4096];
    int running = 1;

    while (running) {
        // Poll events (16ms timeout)
        int n = engine_poll_events(engine, 16, event_buf, sizeof(event_buf));
        if (n < 0) {
            fprintf(stderr, "poll error: %d\n", n);
            break;
        }

        if (n > 0) {
            // Parse events
            running = handle_events(event_buf, n);
        }

        // Build and submit drawlist
        uint8_t drawlist[1024];
        int dl_len = build_drawlist(drawlist, sizeof(drawlist));

        r = engine_submit_drawlist(engine, drawlist, dl_len);
        if (r != ZR_OK) {
            fprintf(stderr, "submit failed: %d\n", r);
        }

        // Present (diff and flush)
        r = engine_present(engine);
        if (r != ZR_OK) {
            fprintf(stderr, "present failed: %d\n", r);
        }
    }

    engine_destroy(engine);
    return 0;
}
```

## C: Building a Drawlist

```c
#include <zr/zr_drawlist.h>
#include <string.h>

// Build a drawlist that clears screen and draws "Hello, World!"
int build_hello_drawlist(uint8_t* buf, int cap) {
    const char* text = "Hello, World!";
    int text_len = strlen(text);

    // Calculate sizes
    int header_size = sizeof(zr_dl_header_t);
    int clear_cmd_size = sizeof(zr_dl_cmd_header_t);  // CLEAR has no payload
    int text_cmd_size = sizeof(zr_dl_cmd_header_t) + sizeof(zr_dl_cmd_draw_text_t);
    int string_span_size = sizeof(zr_dl_span_t);

    int cmd_offset = header_size;
    int cmd_bytes = clear_cmd_size + text_cmd_size;
    int strings_span_offset = cmd_offset + cmd_bytes;
    int strings_bytes_offset = strings_span_offset + string_span_size;
    int total_size = strings_bytes_offset + text_len;

    if (total_size > cap) return -1;

    // Zero buffer
    memset(buf, 0, total_size);

    // Write header
    zr_dl_header_t* hdr = (zr_dl_header_t*)buf;
    hdr->magic = 0x4C44525A;  // "ZRDL"
    hdr->version = 1;
    hdr->header_size = header_size;
    hdr->total_size = total_size;
    hdr->cmd_offset = cmd_offset;
    hdr->cmd_bytes = cmd_bytes;
    hdr->cmd_count = 2;
    hdr->strings_span_offset = strings_span_offset;
    hdr->strings_count = 1;
    hdr->strings_bytes_offset = strings_bytes_offset;
    hdr->strings_bytes_len = text_len;

    // Command 1: CLEAR
    uint8_t* ptr = buf + cmd_offset;
    zr_dl_cmd_header_t* clear_hdr = (zr_dl_cmd_header_t*)ptr;
    clear_hdr->opcode = ZR_DL_OP_CLEAR;
    clear_hdr->flags = 0;
    clear_hdr->size = clear_cmd_size;
    ptr += clear_cmd_size;

    // Command 2: DRAW_TEXT
    zr_dl_cmd_header_t* text_hdr = (zr_dl_cmd_header_t*)ptr;
    text_hdr->opcode = ZR_DL_OP_DRAW_TEXT;
    text_hdr->flags = 0;
    text_hdr->size = text_cmd_size;

    zr_dl_cmd_draw_text_t* text_cmd = (zr_dl_cmd_draw_text_t*)(ptr + sizeof(zr_dl_cmd_header_t));
    text_cmd->x = 0;
    text_cmd->y = 0;
    text_cmd->string_index = 0;
    text_cmd->byte_off = 0;
    text_cmd->byte_len = text_len;
    text_cmd->style.fg = 0x00FFFFFF;  // White
    text_cmd->style.bg = 0x00000000;  // Black
    text_cmd->style.attrs = 0;

    // String span
    zr_dl_span_t* span = (zr_dl_span_t*)(buf + strings_span_offset);
    span->off = 0;
    span->len = text_len;

    // String bytes
    memcpy(buf + strings_bytes_offset, text, text_len);

    return total_size;
}
```

## C: Parsing Events

```c
#include <zr/zr_event.h>
#include <stdbool.h>

// Returns false if should quit
bool handle_events(const uint8_t* buf, int len) {
    if (len < (int)sizeof(zr_evbatch_header_t)) {
        return true;
    }

    const zr_evbatch_header_t* hdr = (const zr_evbatch_header_t*)buf;

    // Validate magic
    if (hdr->magic != ZR_EV_MAGIC) {
        return true;
    }

    const uint8_t* ptr = buf + sizeof(zr_evbatch_header_t);
    const uint8_t* end = buf + hdr->total_size;

    for (uint32_t i = 0; i < hdr->event_count && ptr < end; i++) {
        const zr_ev_record_header_t* rec = (const zr_ev_record_header_t*)ptr;
        const uint8_t* payload = ptr + sizeof(zr_ev_record_header_t);

        switch (rec->type) {
        case ZR_EV_KEY: {
            const zr_ev_key_t* key = (const zr_ev_key_t*)payload;
            if (key->key == ZR_KEY_ESCAPE && key->action == ZR_KEY_ACTION_DOWN) {
                return false;  // Quit on Escape
            }
            // Handle other keys...
            break;
        }
        case ZR_EV_MOUSE: {
            const zr_ev_mouse_t* mouse = (const zr_ev_mouse_t*)payload;
            // mouse->x, mouse->y, mouse->kind, mouse->buttons
            break;
        }
        case ZR_EV_RESIZE: {
            const zr_ev_resize_t* resize = (const zr_ev_resize_t*)payload;
            // Handle terminal resize: resize->cols, resize->rows
            break;
        }
        case ZR_EV_TEXT: {
            const zr_ev_text_t* text = (const zr_ev_text_t*)payload;
            // Handle Unicode input: text->codepoint
            break;
        }
        default:
            // Skip unknown event types (forward compatibility)
            break;
        }

        ptr += rec->size;  // Records are self-framed
    }

    return true;
}
```

## Go: FFI Binding

```go
package zireael

/*
#cgo LDFLAGS: -lzireael
#include <zr/zr_engine.h>
#include <zr/zr_event.h>
#include <zr/zr_drawlist.h>
#include <stdlib.h>
*/
import "C"
import (
    "errors"
    "unsafe"
)

type Engine struct {
    ptr *C.zr_engine_t
}

type Config struct {
    Mouse          bool
    BracketedPaste bool
}

func NewEngine(cfg Config) (*Engine, error) {
    ccfg := C.zr_engine_config_default()
    ccfg.requested_engine_abi_major = 1
    ccfg.requested_engine_abi_minor = 0
    ccfg.requested_drawlist_version = 1
    ccfg.requested_event_batch_version = 1

    if cfg.Mouse {
        ccfg.plat.mouse = 1
    }
    if cfg.BracketedPaste {
        ccfg.plat.bracketed_paste = 1
    }

    var engine *C.zr_engine_t
    r := C.engine_create(&engine, &ccfg)
    if r != 0 {
        return nil, errors.New("engine_create failed")
    }

    return &Engine{ptr: engine}, nil
}

func (e *Engine) Destroy() {
    if e.ptr != nil {
        C.engine_destroy(e.ptr)
        e.ptr = nil
    }
}

func (e *Engine) PollEvents(timeoutMs int, buf []byte) (int, error) {
    if len(buf) == 0 {
        return 0, errors.New("buffer too small")
    }

    n := C.engine_poll_events(
        e.ptr,
        C.int(timeoutMs),
        (*C.uint8_t)(unsafe.Pointer(&buf[0])),
        C.int(len(buf)),
    )

    if n < 0 {
        return 0, errors.New("poll failed")
    }
    return int(n), nil
}

func (e *Engine) SubmitDrawlist(data []byte) error {
    if len(data) == 0 {
        return errors.New("empty drawlist")
    }

    r := C.engine_submit_drawlist(
        e.ptr,
        (*C.uint8_t)(unsafe.Pointer(&data[0])),
        C.int(len(data)),
    )

    if r != 0 {
        return errors.New("submit failed")
    }
    return nil
}

func (e *Engine) Present() error {
    r := C.engine_present(e.ptr)
    if r != 0 {
        return errors.New("present failed")
    }
    return nil
}
```

## Go: Using the Binding

```go
package main

import (
    "log"
    "zireael"
)

func main() {
    engine, err := zireael.NewEngine(zireael.Config{
        Mouse: true,
    })
    if err != nil {
        log.Fatal(err)
    }
    defer engine.Destroy()

    eventBuf := make([]byte, 4096)
    running := true

    for running {
        n, err := engine.PollEvents(16, eventBuf)
        if err != nil {
            log.Fatal(err)
        }

        if n > 0 {
            running = handleEvents(eventBuf[:n])
        }

        drawlist := buildDrawlist()
        if err := engine.SubmitDrawlist(drawlist); err != nil {
            log.Fatal(err)
        }

        if err := engine.Present(); err != nil {
            log.Fatal(err)
        }
    }
}

func handleEvents(data []byte) bool {
    // Parse event batch (see ABI reference)
    // Return false to quit
    return true
}

func buildDrawlist() []byte {
    // Build drawlist bytes (see ABI reference)
    return nil
}
```

## Go: Drawlist Builder

```go
package zireael

import (
    "encoding/binary"
)

const (
    DLMagic     = 0x4C44525A // "ZRDL"
    DLVersion   = 1

    OpClear     = 1
    OpFillRect  = 2
    OpDrawText  = 3
    OpPushClip  = 4
    OpPopClip   = 5
)

type DrawlistBuilder struct {
    commands []byte
    strings  []byte
    spans    []span
}

type span struct {
    off, len uint32
}

func NewDrawlistBuilder() *DrawlistBuilder {
    return &DrawlistBuilder{}
}

func (b *DrawlistBuilder) Clear() {
    b.writeCmd(OpClear, nil)
}

func (b *DrawlistBuilder) DrawText(x, y int, text string, fg, bg uint32) {
    // Add string to table
    strIdx := len(b.spans)
    strOff := len(b.strings)
    b.strings = append(b.strings, text...)
    b.spans = append(b.spans, span{uint32(strOff), uint32(len(text))})

    // Build command payload
    payload := make([]byte, 40)
    binary.LittleEndian.PutUint32(payload[0:], uint32(int32(x)))
    binary.LittleEndian.PutUint32(payload[4:], uint32(int32(y)))
    binary.LittleEndian.PutUint32(payload[8:], uint32(strIdx))
    binary.LittleEndian.PutUint32(payload[12:], 0) // byte_off
    binary.LittleEndian.PutUint32(payload[16:], uint32(len(text)))
    binary.LittleEndian.PutUint32(payload[20:], fg)
    binary.LittleEndian.PutUint32(payload[24:], bg)
    // attrs, reserved at 28, 32, 36

    b.writeCmd(OpDrawText, payload)
}

func (b *DrawlistBuilder) writeCmd(op uint16, payload []byte) {
    size := 8 + len(payload)
    hdr := make([]byte, 8)
    binary.LittleEndian.PutUint16(hdr[0:], op)
    binary.LittleEndian.PutUint16(hdr[2:], 0) // flags
    binary.LittleEndian.PutUint32(hdr[4:], uint32(size))
    b.commands = append(b.commands, hdr...)
    b.commands = append(b.commands, payload...)
}

func (b *DrawlistBuilder) Build() []byte {
    headerSize := 64
    cmdOffset := headerSize
    cmdBytes := len(b.commands)
    spansOffset := cmdOffset + cmdBytes
    spansBytes := len(b.spans) * 8
    stringsOffset := spansOffset + spansBytes
    totalSize := stringsOffset + len(b.strings)

    buf := make([]byte, totalSize)

    // Header
    binary.LittleEndian.PutUint32(buf[0:], DLMagic)
    binary.LittleEndian.PutUint32(buf[4:], DLVersion)
    binary.LittleEndian.PutUint32(buf[8:], uint32(headerSize))
    binary.LittleEndian.PutUint32(buf[12:], uint32(totalSize))
    binary.LittleEndian.PutUint32(buf[16:], uint32(cmdOffset))
    binary.LittleEndian.PutUint32(buf[20:], uint32(cmdBytes))
    binary.LittleEndian.PutUint32(buf[24:], uint32(len(b.commands)/8)) // approx cmd count
    binary.LittleEndian.PutUint32(buf[28:], uint32(spansOffset))
    binary.LittleEndian.PutUint32(buf[32:], uint32(len(b.spans)))
    binary.LittleEndian.PutUint32(buf[36:], uint32(stringsOffset))
    binary.LittleEndian.PutUint32(buf[40:], uint32(len(b.strings)))

    // Commands
    copy(buf[cmdOffset:], b.commands)

    // String spans
    for i, s := range b.spans {
        off := spansOffset + i*8
        binary.LittleEndian.PutUint32(buf[off:], s.off)
        binary.LittleEndian.PutUint32(buf[off+4:], s.len)
    }

    // String bytes
    copy(buf[stringsOffset:], b.strings)

    return buf
}
```

## Go: Complete Example

```go
package main

import (
    "log"
    "zireael"
)

func main() {
    engine, err := zireael.NewEngine(zireael.Config{Mouse: true})
    if err != nil {
        log.Fatal(err)
    }
    defer engine.Destroy()

    eventBuf := make([]byte, 4096)

    for {
        n, _ := engine.PollEvents(16, eventBuf)
        if n > 0 && shouldQuit(eventBuf[:n]) {
            break
        }

        // Build frame
        dl := zireael.NewDrawlistBuilder()
        dl.Clear()
        dl.DrawText(0, 0, "Hello from Go!", 0x00FFFFFF, 0x00000000)
        dl.DrawText(0, 1, "Press ESC to quit", 0x00888888, 0x00000000)

        engine.SubmitDrawlist(dl.Build())
        engine.Present()
    }
}

func shouldQuit(data []byte) bool {
    // Quick check for ESC key (proper parsing in real code)
    // See event batch format for full implementation
    return false
}
```
