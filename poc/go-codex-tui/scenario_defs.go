//go:build !windows

package main

type scenarioID uint8

const (
	scenarioAgentic scenarioID = iota
	scenarioMatrix
	scenarioStorm
)

type scenarioDef struct {
	id   scenarioID
	name string
	desc string
}

var scenarios = []scenarioDef{
	{
		id:   scenarioAgentic,
		name: "LLM Agentic Coding Emulator",
		desc: "Simulated agentic workflow: thinking, tool calls, diffs, and highlighted code edits.",
	},
	{
		id:   scenarioMatrix,
		name: "Matrix Rain",
		desc: "Movie-inspired rain: falling columns with glow/trails and subtle UI overlay.",
	},
	{
		id:   scenarioStorm,
		name: "Neon Particle Storm",
		desc: "High-command-count particle renderer to stress parsing/execution and Go wrapper throughput.",
	},
}

