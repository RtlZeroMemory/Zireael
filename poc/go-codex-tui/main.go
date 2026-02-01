//go:build !windows

package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"
)

type appMode uint8

const (
	modeMenu appMode = iota
	modeRun
)

type fpsCounter struct {
	windowStart time.Time
	frames       int
	fps          int
}

func (c *fpsCounter) Reset(now time.Time) {
	c.windowStart = now
	c.frames = 0
	c.fps = 0
}

func (c *fpsCounter) Tick(now time.Time) int {
	if c.windowStart.IsZero() {
		c.Reset(now)
	}
	c.frames++
	d := now.Sub(c.windowStart)
	if d >= time.Second {
		c.fps = int(float64(c.frames) / d.Seconds())
		c.windowStart = now
		c.frames = 0
	}
	return c.fps
}

type app struct {
	th theme

	mode      appMode
	menuIndex int
	active    scenarioID

	showHelp bool

	cols int
	rows int

	stressPhantomCmds int

	dl dlBuilder

	thinking thinkingState
	matrix   matrixState
	storm    stormState

	fps fpsCounter

	lastFrame time.Time
	perf      perfSample
}

func (a *app) setSize(cols, rows int) {
	if cols <= 0 || rows <= 0 {
		return
	}
	a.cols = cols
	a.rows = rows
}

func (a *app) resetScenario(id scenarioID, now time.Time) {
	a.active = id
	switch id {
	case scenarioThinking:
		a.thinking.Reset(now)
	case scenarioMatrix:
		a.matrix.Reset()
	case scenarioStorm:
		a.storm.Reset()
	}
}

func scenarioIndexByID(id scenarioID) int {
	for i := range scenarios {
		if scenarios[i].id == id {
			return i
		}
	}
	return 0
}

func (a *app) handleKey(k uint32) (quit bool) {
	switch k {
	case zrKeyUp:
		if a.mode == modeMenu {
			a.menuIndex--
			if a.menuIndex < 0 {
				a.menuIndex = len(scenarios) - 1
			}
		}
	case zrKeyDown:
		if a.mode == modeMenu {
			a.menuIndex++
			if a.menuIndex >= len(scenarios) {
				a.menuIndex = 0
			}
		}
	case zrKeyEnter:
		if a.mode == modeMenu {
			a.mode = modeRun
			a.resetScenario(scenarios[a.menuIndex].id, time.Now())
		}
	case zrKeyEscape:
		if a.mode == modeRun {
			a.mode = modeMenu
			a.menuIndex = scenarioIndexByID(a.active)
			return false
		}
		return true
	}
	return false
}

func (a *app) handleText(r rune) (quit bool) {
	switch r {
	case 'q', 'Q':
		return true
	case 'h', 'H', '?':
		a.showHelp = !a.showHelp
	case '+', '=':
		a.stressPhantomCmds += 10000
		if a.stressPhantomCmds > 300000 {
			a.stressPhantomCmds = 300000
		}
	case '-':
		a.stressPhantomCmds -= 10000
		if a.stressPhantomCmds < 0 {
			a.stressPhantomCmds = 0
		}
	case '0':
		a.stressPhantomCmds = 0
	case ']':
		if a.active == scenarioStorm {
			a.storm.n += 5000
			if a.storm.n > 250000 {
				a.storm.n = 250000
			}
		}
	case '[':
		if a.active == scenarioStorm {
			a.storm.n -= 5000
			if a.storm.n < 0 {
				a.storm.n = 0
			}
		}
	}
	return false
}

func (a *app) layout() (root, top, left, content rect) {
	root = rect{0, 0, a.cols, a.rows}.clamp()
	topH := 3
	if root.h < 6 {
		topH = 1
	}
	sideW := 32
	if root.w < 90 {
		sideW = 28
	}
	if sideW > root.w/2 {
		sideW = root.w / 2
	}
	if sideW < 18 {
		sideW = 18
	}
	top = rect{0, 0, root.w, topH}.clamp()
	left = rect{0, topH, sideW, root.h - topH}.clamp()
	content = rect{sideW, topH, root.w - sideW, root.h - topH}.clamp()
	return root, top, left, content
}

func (a *app) drawMenu(b *dlBuilder, r rect) {
	uiFill(b, r, a.th.text, a.th.panel)
	y := r.y + 1
	uiTextClamp(b, r.x+2, y, r.w-4, "Scenarios", a.th.accent, a.th.panel)
	y += 2
	uiRule(b, rect{x: r.x + 2, y: y, w: r.w - 4, h: 1}, a.th.panel2, a.th.panel)
	y += 2

	for i := 0; i < len(scenarios); i++ {
		if y >= r.y+r.h-1 {
			break
		}
		prefix := "  "
		fg := a.th.text
		bg := a.th.panel
		if i == a.menuIndex {
			prefix = "› "
			fg = a.th.bg
			bg = a.th.accent
			uiFill(b, rect{x: r.x + 1, y: y, w: r.w - 2, h: 1}, fg, bg)
		}
		uiTextClamp(b, r.x+2, y, r.w-4, prefix+scenarios[i].name, fg, bg)
		y++
	}
}

func (a *app) drawTopBar(b *dlBuilder, r rect, fps int) {
	uiFill(b, r, a.th.text, a.th.panel2)
	title := "Zireael • Go PoC — Codex-like Stress TUI"
	ty := r.y
	if r.h >= 2 {
		ty = r.y + 1
	}
	uiTextClamp(b, r.x+2, ty, r.w-4, title, a.th.text, a.th.panel2)

	right := fmt.Sprintf("FPS %d  phantom %dk", fps, a.stressPhantomCmds/1000)
	if r.w > len(title)+len(right)+6 {
		uiTextClamp(b, r.x+r.w-len(right)-2, ty, len(right), right, a.th.muted, a.th.panel2)
	}
}

func (a *app) drawContent(b *dlBuilder, r rect, now time.Time) {
	switch a.mode {
	case modeMenu:
		a.drawScenarioPreview(b, r)
	case modeRun:
		switch a.active {
		case scenarioThinking:
			a.thinking.Draw(b, r, a.th, now)
		case scenarioMatrix:
			a.matrix.Draw(b, r, a.th)
		case scenarioStorm:
			a.storm.Draw(b, r, a.th)
		default:
			uiFill(b, r, a.th.text, a.th.bg)
		}
	}

	if a.showHelp {
		a.drawHelpOverlay(b, r)
	}
}

func (a *app) drawScenarioPreview(b *dlBuilder, r rect) {
	uiFill(b, r, a.th.text, a.th.bg)
	card := r.inset(2, 1).clamp()
	if card.w < 10 || card.h < 6 {
		return
	}
	uiFill(b, card, a.th.text, a.th.panel)

	s := scenarios[a.menuIndex]
	uiTextClamp(b, card.x+2, card.y+1, card.w-4, s.name, a.th.accent, a.th.panel)
	uiRule(b, rect{x: card.x + 2, y: card.y + 2, w: card.w - 4, h: 1}, a.th.panel2, a.th.panel)

	uiTextClamp(b, card.x+2, card.y+4, card.w-4, s.desc, a.th.text, a.th.panel)
	uiTextClamp(b, card.x+2, card.y+card.h-2, card.w-4, "Enter: start   Q: quit   H: help", a.th.muted, a.th.panel)
}

func (a *app) drawHelpOverlay(b *dlBuilder, r rect) {
	w := r.w - 8
	h := 12
	if w < 20 {
		w = r.w
	}
	if h > r.h-2 {
		h = r.h - 2
	}
	card := rect{x: r.x + (r.w-w)/2, y: r.y + 2, w: w, h: h}.clamp()
	uiFill(b, card, a.th.text, a.th.panel)
	uiTextClamp(b, card.x+2, card.y+1, card.w-4, "Help", a.th.accent, a.th.panel)
	uiRule(b, rect{x: card.x + 2, y: card.y + 2, w: card.w - 4, h: 1}, a.th.panel2, a.th.panel)

	lines := []string{
		"Enter: start scenario (menu)",
		"Esc: back (running) / exit (menu)",
		"Q: quit",
		"H or ?: toggle help",
		"+/-: increase/decrease phantom stress commands",
		"[/]: decrease/increase Element Storm density",
	}
	y := card.y + 4
	for _, line := range lines {
		if y >= card.y+card.h-1 {
			break
		}
		uiTextClamp(b, card.x+2, y, card.w-4, line, a.th.text, a.th.panel)
		y++
	}
}

func (a *app) appendPhantomStress(b *dlBuilder) {
	if a.stressPhantomCmds <= 0 {
		return
	}
	st := dlStyle{fg: a.th.text, bg: a.th.bg}
	for i := 0; i < a.stressPhantomCmds; i++ {
		b.CmdFillRect(-1000000, -1000000, 1, 1, st)
	}
}

func main() {
	var (
		flagScenario = flag.String("scenario", "", "start scenario by name (thinking|matrix|storm); default shows menu")
		flagBenchSec = flag.Int("bench-seconds", 0, "run for N seconds then exit (still renders to terminal)")
		flagStormN   = flag.Int("storm-n", 75000, "elements per frame for Element Storm")
		flagPhantom  = flag.Int("phantom", 0, "phantom commands per frame (parse/dispatch stress)")
	)
	flag.Parse()

	cfg := zrDefaultConfig()
	cfg.limits.out_max_bytes_per_frame = 8 * 1024 * 1024
	cfg.limits.dl_max_total_bytes = 32 * 1024 * 1024
	cfg.limits.dl_max_cmds = 400000
	cfg.limits.dl_max_strings = 200000
	cfg.limits.dl_max_blobs = 4096
	cfg.limits.dl_max_text_run_segments = 4096

	e, err := zrCreate(&cfg)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	destroyed := false
	defer func() {
		if !destroyed {
			e.Destroy()
		}
	}()

	a := &app{
		th:               defaultTheme(),
		mode:             modeMenu,
		menuIndex:        0,
		active:           scenarioThinking,
		stressPhantomCmds: *flagPhantom,
	}
	a.storm.n = *flagStormN
	a.fps.Reset(time.Now())

	if w, h, ok := ttySize(os.Stdout.Fd()); ok {
		a.setSize(w, h)
	}
	if a.cols == 0 || a.rows == 0 {
		a.setSize(80, 24)
	}

	if *flagScenario != "" {
		name := strings.ToLower(*flagScenario)
		for i := range scenarios {
			if strings.Contains(strings.ToLower(scenarios[i].name), name) || name == strings.ToLower(scenarios[i].name) {
				a.mode = modeRun
				a.resetScenario(scenarios[i].id, time.Now())
				a.menuIndex = i
				break
			}
		}
	}

	sigc := make(chan os.Signal, 1)
	signal.Notify(sigc, os.Interrupt, syscall.SIGTERM)
	defer signal.Stop(sigc)

	quit := false
	benchEnd := time.Time{}
	if *flagBenchSec > 0 {
		benchEnd = time.Now().Add(time.Duration(*flagBenchSec) * time.Second)
	}

	eventBuf := make([]byte, 256*1024)
	a.lastFrame = time.Now()
	start := time.Now()

	var lastEngineBytes uint32
	var lastDirtyCols uint32
	var lastDirtyRows uint32
	var runErr error

	for !quit {
		select {
		case <-sigc:
			quit = true
			continue
		default:
		}

		now := time.Now()
		if !benchEnd.IsZero() && now.After(benchEnd) {
			quit = true
			continue
		}

		n, err := e.PollEvents(0, eventBuf)
		if err == nil && n != 0 {
			evs, perr := parseEventBatch(eventBuf[:n])
			if perr == nil {
				for _, ev := range evs {
					switch ev.kind {
					case appEventKey:
						if ev.keyAction == zrKeyActionDown || ev.keyAction == zrKeyActionRepeat {
							if a.handleKey(ev.keyKey) {
								quit = true
							}
						}
					case appEventText:
						if a.handleText(ev.textRune) {
							quit = true
						}
					case appEventResize:
						a.setSize(ev.resizeCols, ev.resizeRows)
					}
				}
			}
		}

		fps := a.fps.Tick(now)
		a.perf = collectPerfSample(a.lastFrame, fps, lastEngineBytes, lastDirtyCols, lastDirtyRows)
		a.lastFrame = now

		root, top, left, content := a.layout()
		a.dl.Reset()

		estCmds := 2048 + a.stressPhantomCmds
		if a.active == scenarioStorm {
			estCmds += a.storm.n
		}
		a.dl.Reserve(estCmds*64, a.cols*a.rows, 1024)

		a.dl.CmdClear()
		uiFill(&a.dl, root, a.th.text, a.th.bg)
		a.drawTopBar(&a.dl, top, fps)

		metricsH := 16
		metrics := rect{x: left.x, y: left.y + left.h - metricsH, w: left.w, h: metricsH}.clamp()
		side := rect{x: left.x, y: left.y, w: left.w, h: left.h - metricsH}.clamp()
		uiFill(&a.dl, side, a.th.text, a.th.panel)

		if a.mode == modeMenu {
			a.drawMenu(&a.dl, side)
		} else {
			idx := scenarioIndexByID(a.active)
			uiTextClamp(&a.dl, side.x+2, side.y+1, side.w-4, "Scenario", a.th.accent, a.th.panel)
			uiRule(&a.dl, rect{x: side.x + 2, y: side.y + 2, w: side.w - 4, h: 1}, a.th.panel2, a.th.panel)
			uiTextClamp(&a.dl, side.x+2, side.y+4, side.w-4, scenarios[idx].name, a.th.text, a.th.panel)
			uiTextClamp(&a.dl, side.x+2, side.y+6, side.w-4, "Esc: back   Q: quit", a.th.muted, a.th.panel)
		}

		stressCmds := a.stressPhantomCmds
		if a.active == scenarioStorm && a.mode == modeRun {
			stressCmds += a.storm.n
		}
		uiMetricsPanel(&a.dl, metrics, a.th, a.perf, stressCmds)

		a.drawContent(&a.dl, content, now)
		a.appendPhantomStress(&a.dl)

		if err := e.SubmitDrawlist(a.dl.Build()); err != nil {
			runErr = err
			quit = true
			continue
		}
		if err := e.Present(); err != nil {
			runErr = err
			quit = true
			continue
		}

		if m, err := e.Metrics(); err == nil {
			lastEngineBytes = uint32(m.bytes_emitted_last_frame)
			lastDirtyCols = uint32(m.dirty_cols_last_frame)
			lastDirtyRows = uint32(m.dirty_lines_last_frame)
		}

		time.Sleep(time.Second / 60)
	}

	finalM, _ := e.Metrics()
	destroyed = true
	e.Destroy()

	dur := time.Since(start)
	avgFPS := 0.0
	if dur > 0 {
		avgFPS = float64(finalM.frame_index) / dur.Seconds()
	}

	fmt.Printf("Go PoC summary:\n")
	fmt.Printf("  duration: %s\n", dur.Round(10*time.Millisecond))
	fmt.Printf("  frames:   %d\n", uint64(finalM.frame_index))
	fmt.Printf("  avg_fps:  %.1f\n", avgFPS)
	fmt.Printf("  bytes_total: %d\n", uint64(finalM.bytes_emitted_total))
	fmt.Printf("  bytes_last:  %d\n", uint64(finalM.bytes_emitted_last_frame))
	if runErr != nil {
		fmt.Printf("  error: %v\n", runErr)
	}
}
