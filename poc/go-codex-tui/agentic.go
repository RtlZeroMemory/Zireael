//go:build !windows

package main

import (
	"fmt"
	"strings"
	"time"
)

type agenticState struct {
	start time.Time

	doc      []string
	diff     []string
	toolLog  []string
	curLine  int
	curPhase int
}

func (s *agenticState) Reset(now time.Time) {
	s.start = now
	s.doc = agenticEditorDocument()
	s.diff = agenticDiffDocument()
	s.toolLog = agenticToolLog()
	s.curLine = 0
	s.curPhase = 0
}

type agenticPhase struct {
	title string
	kind  string
	body  []string
}

func agenticPhases() []agenticPhase {
	return []agenticPhase{
		{
			title: "Thinking",
			kind:  "analysis",
			body: []string{
				"Goal: reduce write() failures under backpressure",
				"Approach:",
				"  - treat EAGAIN/EWOULDBLOCK as transient",
				"  - poll() for POLLOUT then continue",
				"",
				"Constraints:",
				"  - single flush per present",
				"  - platform boundary respected",
			},
		},
		{
			title: "Tool Call",
			kind:  "rg",
			body: []string{
				"$ rg -n \"zr_posix_write_all\" src/platform/posix/zr_plat_posix.c",
				"185: static zr_result_t zr_posix_write_all(int fd, const uint8_t* bytes, int32_t len) {",
				"",
				"Found 1 match.",
			},
		},
		{
			title: "Tool Call",
			kind:  "apply_patch",
			body: []string{
				"$ apply_patch",
				"*** Begin Patch",
				"*** Update File: src/platform/posix/zr_plat_posix.c",
				"+ if (errno == EAGAIN || errno == EWOULDBLOCK) {",
				"+   /* wait for POLLOUT then retry */",
				"+ }",
				"*** End Patch",
				"",
				"Applied patch successfully.",
			},
		},
		{
			title: "Tool Call",
			kind:  "build",
			body: []string{
				"$ cmake --build --preset posix-clang-debug",
				"[1/10] Building C object ...",
				"[2/10] Linking C static library libzireael.a",
				"...",
				"Build succeeded.",
			},
		},
		{
			title: "Tool Call",
			kind:  "test",
			body: []string{
				"$ ctest --test-dir out/build/posix-clang-debug --output-on-failure",
				"100% tests passed, 0 tests failed out of 9",
			},
		},
	}
}

func (s *agenticState) phase(now time.Time) agenticPhase {
	p := agenticPhases()
	if s.start.IsZero() {
		s.start = now
	}
	elapsed := now.Sub(s.start)
	stepMs := int(elapsed / (2200 * time.Millisecond))
	return p[stepMs%len(p)]
}

func agenticFileTree() []string {
	return []string{
		"src/",
		"  core/",
		"    zr_engine.c",
		"    zr_diff.c",
		"  platform/",
		"    posix/",
		"      zr_plat_posix.c",
		"    win32/",
		"      zr_plat_win32.c",
		"tests/",
		"  integration/",
		"  unit/",
	}
}

func agenticDiffDocument() []string {
	return []string{
		"diff --git a/src/platform/posix/zr_plat_posix.c b/src/platform/posix/zr_plat_posix.c",
		"index 1234567..89abcde 100644",
		"--- a/src/platform/posix/zr_plat_posix.c",
		"+++ b/src/platform/posix/zr_plat_posix.c",
		"@@ -200,6 +236,18 @@ static zr_result_t zr_posix_write_all(int fd, const uint8_t* bytes, int32_t len) {",
		"+    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {",
		"+      /* stdout may be non-blocking under wrappers; wait for POLLOUT and retry */",
		"+      zr_result_t rc = zr_posix_wait_writable(fd);",
		"+      if (rc != ZR_OK) {",
		"+        return rc;",
		"+      }",
		"+      continue;",
		"+    }",
		"     if (n < 0 && errno == EINTR) {",
		"       continue;",
		"     }",
		"     return ZR_ERR_PLATFORM;",
		"   }",
	}
}

func agenticToolLog() []string {
	return []string{
		"$ rg -n \"plat_write_output\" src/platform/posix/zr_plat_posix.c",
		"455: zr_result_t plat_write_output(plat_t* plat, const uint8_t* bytes, int32_t len) {",
		"",
		"$ apply_patch",
		"*** Begin Patch",
		"*** Update File: src/platform/posix/zr_plat_posix.c",
		"+ if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {",
		"+   zr_result_t rc = zr_posix_wait_writable(fd);",
		"+   if (rc != ZR_OK) {",
		"+     return rc;",
		"+   }",
		"+   continue;",
		"+ }",
		"*** End Patch",
		"",
		"$ cmake --build --preset posix-clang-debug",
		"[1/10] Building C object ...",
		"[2/10] Linking C static library libzireael.a",
		"",
		"$ ctest --test-dir out/build/posix-clang-debug --output-on-failure",
		"100% tests passed, 0 tests failed out of 9",
	}
}

func agenticEditorDocument() []string {
	/*
		A larger editor buffer so the viewport can scroll. The content is a
		plausible patch area plus surrounding context.
	*/
	doc := make([]string, 0, 220)
	doc = append(doc,
		"/*",
		"  src/platform/posix/zr_plat_posix.c — POSIX platform backend.",
		"",
		"  Notes:",
		"    - stdout may be non-blocking under wrappers",
		"    - treat EAGAIN as transient backpressure",
		"*/",
		"",
		"static zr_result_t zr_posix_write_all(int fd, const uint8_t* bytes, int32_t len) {",
		"  if (len < 0) {",
		"    return ZR_ERR_INVALID_ARGUMENT;",
		"  }",
		"  if (len == 0) {",
		"    return ZR_OK;",
		"  }",
		"",
		"  int32_t written = 0;",
		"  while (written < len) {",
		"    ssize_t n = write(fd, bytes + (size_t)written, (size_t)(len - written));",
		"    if (n > 0) {",
		"      written += (int32_t)n;",
		"      continue;",
		"    }",
		"    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {",
		"      zr_result_t rc = zr_posix_wait_writable(fd);",
		"      if (rc != ZR_OK) {",
		"        return rc;",
		"      }",
		"      continue;",
		"    }",
		"    if (n < 0 && errno == EINTR) {",
		"      continue;",
		"    }",
		"    return ZR_ERR_PLATFORM;",
		"  }",
		"  return ZR_OK;",
		"}",
	)
	for i := 0; i < 160; i++ {
		doc = append(doc, fmt.Sprintf("/* … context line %03d … */", i))
	}
	doc = append(doc,
		"",
		"zr_result_t plat_write_output(plat_t* plat, const uint8_t* bytes, int32_t len) {",
		"  if (!plat) {",
		"    return ZR_ERR_INVALID_ARGUMENT;",
		"  }",
		"  return zr_posix_write_all(plat->stdout_fd, bytes, len);",
		"}",
	)
	return doc
}

func lexLikeC(line string, th theme) []uiRunSeg {
	if strings.HasPrefix(strings.TrimSpace(line), "//") {
		return []uiRunSeg{{text: line, fg: th.good}}
	}

	kw := map[string]bool{
		"static": true, "return": true, "if": true, "while": true, "continue": true, "const": true,
	}

	out := make([]uiRunSeg, 0, 8)
	cur := ""
	curColor := th.text

	flush := func() {
		if cur == "" {
			return
		}
		out = append(out, uiRunSeg{text: cur, fg: curColor})
		cur = ""
		curColor = th.text
	}

	isIdent := func(b byte) bool {
		return (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9') || b == '_'
	}

	for i := 0; i < len(line); {
		c := line[i]
		if c == '"' {
			flush()
			j := i + 1
			for j < len(line) && line[j] != '"' {
				if line[j] == '\\' && j+1 < len(line) {
					j += 2
					continue
				}
				j++
			}
			if j < len(line) {
				j++
			}
			out = append(out, uiRunSeg{text: line[i:j], fg: rgb(245, 158, 11)})
			i = j
			continue
		}
		if c == '/' && i+1 < len(line) && line[i+1] == '/' {
			flush()
			out = append(out, uiRunSeg{text: line[i:], fg: rgb(34, 197, 94)})
			break
		}
		if isIdent(c) && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
			flush()
			j := i + 1
			for j < len(line) && isIdent(line[j]) {
				j++
			}
			word := line[i:j]
			color := th.text
			if kw[word] {
				color = th.accent2
			} else if strings.HasPrefix(word, "ZR_") || strings.HasPrefix(word, "zr_") {
				color = rgb(56, 189, 248)
			} else if word == "errno" || word == "EINTR" || word == "EAGAIN" || word == "EWOULDBLOCK" {
				color = rgb(251, 113, 133)
			}
			out = append(out, uiRunSeg{text: word, fg: color})
			i = j
			continue
		}
		cur += string(c)
		i++
	}
	flush()
	return out
}

func lexDiff(line string, th theme) []uiRunSeg {
	trim := strings.TrimSpace(line)
	switch {
	case strings.HasPrefix(line, "+"):
		return []uiRunSeg{{text: line, fg: rgb(74, 222, 128)}}
	case strings.HasPrefix(line, "-"):
		return []uiRunSeg{{text: line, fg: rgb(248, 113, 113)}}
	case strings.HasPrefix(trim, "@@"):
		return []uiRunSeg{{text: line, fg: th.accent2}}
	case strings.HasPrefix(trim, "diff ") || strings.HasPrefix(trim, "index "):
		return []uiRunSeg{{text: line, fg: th.muted}}
	default:
		return []uiRunSeg{{text: line, fg: th.text}}
	}
}

func (s *agenticState) Draw(b *dlBuilder, r rect, th theme, now time.Time) {
	uiFill(b, r, th.text, th.bg)
	if r.w < 40 || r.h < 16 {
		return
	}

	phase := s.phase(now)
	elapsed := now.Sub(s.start)
	scroll := int(elapsed / (130 * time.Millisecond))
	if scroll < 0 {
		scroll = 0
	}
	s.curLine = (scroll * 2) % len(s.doc)
	s.curPhase = int(elapsed/(2200*time.Millisecond)) % len(agenticPhases())

	leftW := 22
	rightW := 34
	bottomH := 12
	if leftW > r.w/3 {
		leftW = r.w / 3
	}
	if rightW > r.w/3 {
		rightW = r.w / 3
	}
	if bottomH > r.h/2 {
		bottomH = r.h / 2
	}

	tree := rect{x: r.x, y: r.y, w: leftW, h: r.h - bottomH}.clamp()
	editor := rect{x: r.x + leftW, y: r.y, w: r.w - leftW - rightW, h: r.h - bottomH}.clamp()
	side := rect{x: r.x + r.w - rightW, y: r.y, w: rightW, h: r.h - bottomH}.clamp()
	bottom := rect{x: r.x, y: r.y + r.h - bottomH, w: r.w, h: bottomH}.clamp()

	uiFill(b, tree, th.text, th.panel)
	uiFill(b, editor, th.text, th.bg)
	uiFill(b, side, th.text, th.panel)
	uiFill(b, bottom, th.text, th.panel2)

	uiTextClamp(b, tree.x+2, tree.y+1, tree.w-4, "Workspace", th.accent, th.panel)
	uiRule(b, rect{x: tree.x + 2, y: tree.y + 2, w: tree.w - 4, h: 1}, th.panel2, th.panel)

	ft := agenticFileTree()
	y := tree.y + 4
	for i := 0; i < len(ft) && y < tree.y+tree.h-1; i++ {
		fg := th.muted
		if strings.Contains(ft[i], "zr_plat_posix.c") {
			fg = th.text
		}
		uiTextClamp(b, tree.x+2, y, tree.w-4, ft[i], fg, th.panel)
		y++
	}

	uiFill(b, rect{x: editor.x, y: editor.y, w: editor.w, h: 1}, th.text, th.panel2)
	uiTextClamp(b, editor.x+2, editor.y, editor.w-4, "src/platform/posix/zr_plat_posix.c", th.text, th.panel2)

	edY := editor.y + 2
	edMax := editor.y + editor.h - 1
	viewH := edMax - edY
	if viewH < 1 {
		viewH = 1
	}
	for row := 0; row < viewH; row++ {
		idx := (s.curLine + row) % len(s.doc)
		line := s.doc[idx]
		y := edY + row

		if row == viewH/2 {
			uiFill(b, rect{x: editor.x, y: y, w: editor.w, h: 1}, th.text, rgb(10, 16, 22))
		}

		ln := fmt.Sprintf("%4d", 1+idx)
		uiTextClamp(b, editor.x+1, y, 5, ln, th.muted, th.bg)
		segs := lexLikeC(line, th)
		uiTextRun(b, editor.x+7, y, segs, th.bg)
	}

	uiTextClamp(b, side.x+2, side.y+1, side.w-4, "Agent", th.accent, th.panel)
	uiRule(b, rect{x: side.x + 2, y: side.y + 2, w: side.w - 4, h: 1}, th.panel2, th.panel)

	uiTextClamp(b, side.x+2, side.y+4, side.w-4, phase.title, th.text, th.panel)
	uiTextClamp(b, side.x+2, side.y+5, side.w-4, "mode: "+phase.kind, th.muted, th.panel)

	yy := side.y + 7
	for i := 0; i < len(phase.body) && yy+i < side.y+side.h-1; i++ {
		uiTextClamp(b, side.x+2, yy+i, side.w-4, phase.body[i], th.text, th.panel)
	}

	uiTextClamp(b, bottom.x+2, bottom.y+1, bottom.w-4, "Diff + Tool Output", th.text, th.panel2)
	uiRule(b, rect{x: bottom.x + 2, y: bottom.y + 2, w: bottom.w - 4, h: 1}, th.panel, th.panel2)

	dy := bottom.y + 4
	linesLeft := (bottom.h - 5) / 2
	if linesLeft < 2 {
		linesLeft = 2
	}
	linesRight := bottom.h - 5 - linesLeft

	for i := 0; i < linesLeft && dy+i < bottom.y+bottom.h-1; i++ {
		idx := (s.curPhase + i) % len(s.diff)
		segs := lexDiff(s.diff[idx], th)
		uiTextRun(b, bottom.x+2, dy+i, segs, th.panel2)
	}

	toolY := dy + linesLeft + 1
	uiTextClamp(b, bottom.x+2, toolY-1, bottom.w-4, "Tool log", th.muted, th.panel2)
	for i := 0; i < linesRight && toolY+i < bottom.y+bottom.h-1; i++ {
		idx := (s.curPhase*2 + i) % len(s.toolLog)
		line := s.toolLog[idx]
		if strings.HasPrefix(line, "$ ") {
			uiTextClamp(b, bottom.x+2, toolY+i, bottom.w-4, line, th.accent2, th.panel2)
		} else {
			uiTextClamp(b, bottom.x+2, toolY+i, bottom.w-4, line, th.text, th.panel2)
		}
	}
}
