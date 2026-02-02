package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"runtime"
	"strings"
	"time"
)

type appMode uint8

const (
	modeMenu appMode = iota
	modeRun
)

type fpsCounter struct {
	windowStart time.Time
	frames      int
	fps         int
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

	agentic agenticState
	matrix  matrixRainState
	storm   particleStormState

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
	case scenarioAgentic:
		a.agentic.Reset(now)
	case scenarioMatrix:
		a.matrix.Reset()
	case scenarioStorm:
		a.storm.Reset(now)
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
			a.storm.SetVisibleMax(a.storm.visibleMax + 5000)
			if a.storm.visibleMax > 100000 {
				a.storm.SetVisibleMax(100000)
			}
		}
	case '[':
		if a.active == scenarioStorm {
			a.storm.SetVisibleMax(a.storm.visibleMax - 5000)
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

	if r.h >= 3 {
		uiTextClamp(b, r.x+2, r.y+r.h-2, r.w-4, "↑/↓ select  Enter run  H help  Q quit", a.th.muted, a.th.panel)
	}
}

func (a *app) drawTopBar(b *dlBuilder, r rect, fps int) {
	uiFill(b, r, a.th.text, a.th.panel2)
	title := "Zireael • Go PoC — Stress-test TUI"
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
		case scenarioAgentic:
			a.agentic.Draw(b, r, a.th, now)
		case scenarioMatrix:
			a.matrix.Draw(b, r, a.th, now)
		case scenarioStorm:
			a.storm.Draw(b, r, a.th, now)
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

	logoR := rect{x: r.x, y: r.y, w: r.w, h: r.h - 8}.clamp()
	if logoR.w >= 44 && logoR.h >= 10 {
		uiBrandLogo(b, logoR, a.th)
	}
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
		"[/]: decrease/increase Particle Storm visible density",
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
	st := dlStyle{fg: a.th.text, bg: a.th.bg, attrs: 0}
	for i := 0; i < a.stressPhantomCmds; i++ {
		b.CmdFillRect(-1000000, -1000000, 1, 1, st)
	}
}

func main() {
	var (
		flagScenario = flag.String("scenario", "", "start scenario by name (agentic|matrix|storm); default shows menu")
		flagBenchSec = flag.Int("bench-seconds", 0, "run for N seconds then exit (still renders to terminal)")
		flagStormN   = flag.Int("storm-n", 60000, "total particle segments per frame for Neon Particle Storm (excess becomes phantom stress)")
		flagStormVis = flag.Int("storm-visible", 20000, "max visible particle segments per frame for Neon Particle Storm")
		flagPhantom  = flag.Int("phantom", 0, "phantom commands per frame (parse/dispatch stress)")
		flagFPS      = flag.Int("fps", 0, "target render FPS (0=uncapped)")
	)
	flag.Parse()

	cfg := zrDefaultConfig()
	cfg.plat.requestedColorMode = platColorRGB
	/*
		The demo's input path is intentionally minimal (see zr_input_parser.c) and does
		not implement mouse, focus, or bracketed paste decoding yet. Disable these to
		avoid spurious ESC events from terminals that emit those sequences.
	*/
	cfg.plat.enableMouse = 0
	cfg.plat.enableBracketedPaste = 0
	cfg.plat.enableFocusEvents = 0
	cfg.limits.arenaMaxTotalBytes = 256 * 1024 * 1024
	cfg.limits.arenaInitialBytes = 4 * 1024 * 1024
	cfg.limits.outMaxBytesPerFrame = 16 * 1024 * 1024
	cfg.limits.dlMaxTotalBytes = 64 * 1024 * 1024
	cfg.limits.dlMaxCmds = 800000
	cfg.limits.dlMaxStrings = 400000
	cfg.limits.dlMaxBlobs = 4096
	cfg.limits.dlMaxTextRunSegments = 4096

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
		th:                defaultTheme(),
		mode:              modeMenu,
		menuIndex:         0,
		active:            scenarioAgentic,
		stressPhantomCmds: *flagPhantom,
	}
	a.storm.SetCount(*flagStormN)
	a.storm.SetVisibleMax(*flagStormVis)
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
	signal.Notify(sigc, appSignals()...)
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
	var ms runtime.MemStats
	var goAlloc uint64
	var goSys uint64
	lastMemSample := time.Time{}

	nextDeadline := time.Time{}
	if *flagFPS > 0 {
		nextDeadline = time.Now()
	}

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
		if lastMemSample.IsZero() || now.Sub(lastMemSample) >= 250*time.Millisecond {
			runtime.ReadMemStats(&ms)
			goAlloc = ms.Alloc
			goSys = ms.Sys
			lastMemSample = now
		}
		a.perf = collectPerfSample(now, a.lastFrame, fps, lastEngineBytes, lastDirtyCols, lastDirtyRows, goAlloc, goSys)
		a.lastFrame = now

		root, top, left, content := a.layout()
		a.dl.Reset()

		cmdCount := 256 + a.stressPhantomCmds
		if a.mode == modeRun && a.active == scenarioStorm {
			cmdCount += a.storm.n + 8
		}
		if a.mode == modeRun && a.active == scenarioMatrix {
			cmdCount += a.rows + 16
		}
		if a.mode == modeRun && a.active == scenarioAgentic {
			cmdCount += 512
		}

		cmdBytesCap := cmdCount * 48
		if cmdBytesCap < 64*1024 {
			cmdBytesCap = 64 * 1024
		}
		stringsBytesCap := 64 * 1024
		if a.mode == modeRun && a.active == scenarioMatrix {
			stringsBytesCap = (a.cols * a.rows * 3) + 128*1024
		}
		stringsCap := 2048
		if a.mode == modeRun && a.active == scenarioMatrix {
			stringsCap = a.rows + 4096
		}
		blobsBytesCap := 64 * 1024
		blobsCap := 1024
		if a.mode == modeRun && a.active == scenarioMatrix {
			blobsBytesCap = (a.rows * (a.cols*28 + 16)) + 128*1024
			blobsCap = a.rows + 1024
		}
		if a.mode == modeRun && a.active == scenarioAgentic {
			blobsBytesCap = 256 * 1024
			blobsCap = 2048
		}

		a.dl.Reserve(cmdBytesCap, stringsBytesCap, stringsCap)
		a.dl.ReserveBlobs(blobsBytesCap, blobsCap)

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
			lastEngineBytes = uint32(m.bytesEmittedLast)
			lastDirtyCols = uint32(m.dirtyColsLastFrame)
			lastDirtyRows = uint32(m.dirtyLinesLastFrame)
		}

		if *flagFPS > 0 {
			frameDur := time.Second / time.Duration(*flagFPS)
			if nextDeadline.IsZero() {
				nextDeadline = now.Add(frameDur)
			} else {
				nextDeadline = nextDeadline.Add(frameDur)
			}
			sleep := time.Until(nextDeadline)
			if sleep > 0 {
				time.Sleep(sleep)
			} else if sleep < -250*time.Millisecond {
				nextDeadline = time.Now()
			}
		}
	}

	finalM, _ := e.Metrics()
	destroyed = true
	e.Destroy()

	dur := time.Since(start)
	avgFPS := 0.0
	if dur > 0 {
		avgFPS = float64(finalM.frameIndex) / dur.Seconds()
	}

	fmt.Printf("Go PoC summary:\n")
	fmt.Printf("  duration: %s\n", dur.Round(10*time.Millisecond))
	fmt.Printf("  frames:   %d\n", uint64(finalM.frameIndex))
	fmt.Printf("  avg_fps:  %.1f\n", avgFPS)
	fmt.Printf("  bytes_total: %d\n", uint64(finalM.bytesEmittedTotal))
	fmt.Printf("  bytes_last:  %d\n", uint64(finalM.bytesEmittedLast))
	if runErr != nil {
		fmt.Printf("  error: %v\n", runErr)
	}
}
