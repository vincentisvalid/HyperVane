package emulators

import (
	"context"
	"fmt"
)

type RetroArch struct {
	execPath string
}

func NewRetroArch(execPath string) *RetroArch {
	return &RetroArch{execPath: execPath}
}

func (r *RetroArch) ID() string { return "retroarch" }

func (r *RetroArch) BuildArgs(romPath string, overrides map[string]string) ([]string, error) {
	if romPath == "" {
		return nil, fmt.Errorf("retroarch: rom path is empty")
	}
	args := []string{r.execPath}
	if core, ok := overrides["core"]; ok && core != "" {
		args = append(args, "-L", core)
	}
	if overrides["fullscreen"] == "true" {
		args = append(args, "--fullscreen")
	}
	args = append(args, romPath)
	return args, nil
}

func (r *RetroArch) Configure(_ context.Context, _ map[string]string) error {
	return nil
}
