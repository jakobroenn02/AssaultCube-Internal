//go:build !windows
// +build !windows

package ipc

import (
	"fmt"
	"time"
)

// PipeClient is a placeholder implementation for non-Windows platforms.
type PipeClient struct{}

// Connect reports that named pipe IPC is unavailable on non-Windows platforms.
func Connect(timeout time.Duration) (*PipeClient, error) {
	return nil, fmt.Errorf("named pipe IPC is only supported on Windows")
}

// ReadMessage always returns an error because IPC is unavailable.
func (pc *PipeClient) ReadMessage() (*TrainerMessage, error) {
	return nil, fmt.Errorf("named pipe IPC is only supported on Windows")
}

// Write always returns an error because IPC is unavailable.
func (pc *PipeClient) Write(data []byte) error {
	return fmt.Errorf("named pipe IPC is only supported on Windows")
}

// Close is a no-op for the stub implementation.
func (pc *PipeClient) Close() error {
	return nil
}
