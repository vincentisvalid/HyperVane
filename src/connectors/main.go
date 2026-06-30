package main

import (
	"context"
	"log/slog"
	"os"
	"os/signal"
	"syscall"

	"github.com/hypervane/connectors/ipc"
)

const socketPath = "/tmp/hypervane-connector.sock"

func main() {
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	}))
	slog.SetDefault(logger)

	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer cancel()

	srv := ipc.NewServer(socketPath)
	slog.Info("connector daemon starting", "socket", socketPath)

	if err := srv.Serve(ctx); err != nil {
		slog.Error("server error", "err", err)
		os.Exit(1)
	}
}
