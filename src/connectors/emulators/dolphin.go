package emulators

import (
	"context"
	"fmt"
)

type Dolphin struct {
	execPath string
}

func NewDolphin(execPath string) *Dolphin {
	return &Dolphin{execPath: execPath}
}

func (d *Dolphin) ID() string { return "dolphin" }

func (d *Dolphin) BuildArgs(romPath string, overrides map[string]string) ([]string, error) {
	if romPath == "" {
		return nil, fmt.Errorf("dolphin: rom path is empty")
	}
	args := []string{d.execPath, "--batch", "--exec=" + romPath}
	if overrides["fullscreen"] == "true" {
		args = append(args, "--fullscreen")
	}
	return args, nil
}

func (d *Dolphin) Configure(_ context.Context, _ map[string]string) error {
	return nil
}
