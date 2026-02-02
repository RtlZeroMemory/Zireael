//go:build windows

package main

import "os"

func appSignals() []os.Signal {
	return []os.Signal{os.Interrupt}
}
