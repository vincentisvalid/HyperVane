package ipc

import (
	"context"
	"encoding/binary"
	"fmt"
	"io"
	"log/slog"
	"net"
	"os"
	"sync"

	"google.golang.org/protobuf/proto"

	"github.com/hypervane/connectors/connector"
	"github.com/hypervane/connectors/emulators"
	pb "github.com/hypervane/connectors/proto"
	"github.com/hypervane/connectors/process"
)

// maxMsgLen caps incoming frame size to prevent OOM from malformed clients.
const maxMsgLen = 64 << 20 // 64 MiB

type Server struct {
	socketPath string
	manager    *process.Manager
	connectors map[string]connector.Connector
}

func NewServer(socketPath string) *Server {
	return &Server{
		socketPath: socketPath,
		manager:    process.NewManager(),
		connectors: map[string]connector.Connector{
			"duckstation": emulators.NewDuckStation("duckstation-qt"),
			"retroarch":   emulators.NewRetroArch("retroarch"),
			"dolphin":     emulators.NewDolphin("dolphin-emu"),
		},
	}
}

func (s *Server) Serve(ctx context.Context) error {
	os.Remove(s.socketPath)
	l, err := net.Listen("unix", s.socketPath)
	if err != nil {
		return fmt.Errorf("listen: %w", err)
	}
	defer l.Close()

	go s.broadcastEvents(ctx)

	var wg sync.WaitGroup
	for {
		conn, err := l.Accept()
		if err != nil {
			select {
			case <-ctx.Done():
				wg.Wait()
				s.manager.Wait() // wait for all emulator-watching goroutines
				return nil
			default:
				slog.Error("accept error", "err", err)
				continue
			}
		}
		wg.Add(1)
		go func() {
			defer wg.Done()
			s.handleConn(ctx, conn)
		}()
	}
}

func (s *Server) handleConn(ctx context.Context, conn net.Conn) {
	defer conn.Close()
	for {
		env, err := readEnvelope(conn)
		if err != nil {
			if err != io.EOF {
				slog.Warn("read error", "err", err)
			}
			return
		}
		s.dispatch(ctx, conn, env)
	}
}

func (s *Server) dispatch(ctx context.Context, conn net.Conn, env *pb.Envelope) {
	switch p := env.Payload.(type) {
	case *pb.Envelope_LaunchRequest:
		req := p.LaunchRequest
		c, ok := s.connectors[req.EmulatorId]
		if !ok {
			slog.Warn("unknown emulator", "id", req.EmulatorId)
			return
		}
		if err := c.Configure(ctx, req.Overrides); err != nil {
			slog.Error("configure failed", "emulator", req.EmulatorId, "err", err)
			return
		}
		args, err := c.BuildArgs(req.RomPath, req.Overrides)
		if err != nil {
			slog.Error("build args failed", "emulator", req.EmulatorId, "err", err)
			return
		}
		pid, err := s.manager.Launch(ctx, req.RomId, args)
		if err != nil {
			slog.Error("launch failed", "rom_id", req.RomId, "err", err)
			return
		}
		slog.Info("emulator launched", "pid", pid, "rom_id", req.RomId, "emulator", req.EmulatorId)

	case *pb.Envelope_KillRequest:
		if err := s.manager.Kill(int(p.KillRequest.Pid)); err != nil {
			slog.Warn("kill failed", "err", err)
		}
	}
}

func (s *Server) broadcastEvents(ctx context.Context) {
	for {
		select {
		case <-ctx.Done():
			return
		case ev := <-s.manager.Events():
			slog.Info("process event", "kind", ev.Kind, "pid", ev.PID, "rom_id", ev.RomID)
		}
	}
}

func readEnvelope(r io.Reader) (*pb.Envelope, error) {
	var length uint32
	if err := binary.Read(r, binary.BigEndian, &length); err != nil {
		return nil, err
	}
	if uint64(length) > maxMsgLen {
		return nil, fmt.Errorf("frame length %d exceeds maximum %d", length, maxMsgLen)
	}
	buf := make([]byte, length)
	if _, err := io.ReadFull(r, buf); err != nil {
		return nil, err
	}
	env := &pb.Envelope{}
	return env, proto.Unmarshal(buf, env)
}

func writeEnvelope(w io.Writer, env *pb.Envelope) error {
	b, err := proto.Marshal(env)
	if err != nil {
		return err
	}
	if err := binary.Write(w, binary.BigEndian, uint32(len(b))); err != nil {
		return err
	}
	_, err = w.Write(b)
	return err
}
