package process

import (
	"context"
	"fmt"
	"log/slog"
	"os/exec"
	"sync"
	"time"
)

// Manager launches and tracks emulator child processes.
type Manager struct {
	mu      sync.Mutex
	running map[int]*trackedProcess
	events  chan Event
	wg      sync.WaitGroup // tracks in-flight wait() goroutines
}

type trackedProcess struct {
	cmd     *exec.Cmd
	romID   string
	started time.Time
}

type EventKind int

const (
	EventStarted EventKind = iota
	EventStopped
	EventCrashed
)

type Event struct {
	Kind      EventKind
	PID       int
	RomID     string
	RuntimeMS int64
	CrashInfo string
}

func NewManager() *Manager {
	return &Manager{
		running: make(map[int]*trackedProcess),
		events:  make(chan Event, 64),
	}
}

func (m *Manager) Events() <-chan Event { return m.events }

// Wait blocks until all in-flight wait() goroutines have exited.
func (m *Manager) Wait() { m.wg.Wait() }

func (m *Manager) Launch(ctx context.Context, romID string, args []string) (int, error) {
	if len(args) == 0 {
		return 0, fmt.Errorf("empty args")
	}
	cmd := exec.CommandContext(ctx, args[0], args[1:]...)
	if err := cmd.Start(); err != nil {
		return 0, fmt.Errorf("start: %w", err)
	}
	pid := cmd.Process.Pid
	tp := &trackedProcess{cmd: cmd, romID: romID, started: time.Now()}

	m.mu.Lock()
	m.running[pid] = tp
	m.mu.Unlock()

	m.sendEvent(Event{Kind: EventStarted, PID: pid, RomID: romID})

	m.wg.Add(1)
	go func() {
		defer m.wg.Done()
		m.wait(tp, pid)
	}()

	slog.Info("launched emulator", "pid", pid, "rom_id", romID)
	return pid, nil
}

func (m *Manager) Kill(pid int) error {
	m.mu.Lock()
	tp, ok := m.running[pid]
	m.mu.Unlock()
	if !ok {
		return fmt.Errorf("pid %d not tracked", pid)
	}
	return tp.cmd.Process.Kill()
}

func (m *Manager) wait(tp *trackedProcess, pid int) {
	err := tp.cmd.Wait()
	runtimeMS := time.Since(tp.started).Milliseconds()

	m.mu.Lock()
	delete(m.running, pid)
	m.mu.Unlock()

	kind := EventStopped
	crashInfo := ""
	if err != nil {
		kind = EventCrashed
		crashInfo = err.Error()
	}
	m.sendEvent(Event{
		Kind:      kind,
		PID:       pid,
		RomID:     tp.romID,
		RuntimeMS: runtimeMS,
		CrashInfo: crashInfo,
	})
}

// sendEvent delivers an event without blocking — drops it if the buffer is
// full rather than leaking the goroutine when broadcastEvents has already exited.
func (m *Manager) sendEvent(ev Event) {
	select {
	case m.events <- ev:
	default:
		slog.Warn("event buffer full, dropping event", "kind", ev.Kind, "pid", ev.PID)
	}
}
