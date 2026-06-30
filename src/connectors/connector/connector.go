package connector

import "context"

// Connector is the interface every emulator adapter must satisfy.
type Connector interface {
	// ID returns the canonical emulator identifier (e.g. "duckstation").
	ID() string

	// BuildArgs translates a launch request into the argv slice for exec.
	BuildArgs(romPath string, overrides map[string]string) ([]string, error)

	// Configure writes emulator-specific config files before launch if needed.
	Configure(ctx context.Context, overrides map[string]string) error
}
