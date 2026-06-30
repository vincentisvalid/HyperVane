package emulators

import (
	"context"
	"fmt"
)

type DuckStation struct {
	execPath string
}

func NewDuckStation(execPath string) *DuckStation {
	return &DuckStation{execPath: execPath}
}

func (d *DuckStation) ID() string { return "duckstation" }

func (d *DuckStation) BuildArgs(romPath string, overrides map[string]string) ([]string, error) {
	args := []string{d.execPath, "-batch", "-nogui"}
	if fullscreen, ok := overrides["fullscreen"]; ok && fullscreen == "true" {
		args = append(args, "-fullscreen")
	}
	if romPath == "" {
		return nil, fmt.Errorf("duckstation: rom path is empty")
	}
	args = append(args, "--", romPath)
	return args, nil
}

func (d *DuckStation) Configure(_ context.Context, _ map[string]string) error {
	return nil
}
