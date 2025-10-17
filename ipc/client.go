//go:build windows
// +build windows

package ipc

import (
	"bufio"
	"fmt"
	"os"
	"syscall"
	"time"
	"unsafe"
)

const (
	PIPE_NAME = `\\.\pipe\actrainer_pipe`

	// Named pipe constants
	PIPE_ACCESS_DUPLEX       = 0x00000003
	PIPE_TYPE_MESSAGE        = 0x00000004
	PIPE_READMODE_MESSAGE    = 0x00000002
	PIPE_WAIT                = 0x00000000
	PIPE_UNLIMITED_INSTANCES = 255

	FILE_FLAG_OVERLAPPED = 0x40000000
	GENERIC_READ         = 0x80000000
	GENERIC_WRITE        = 0x40000000
	OPEN_EXISTING        = 3

	ERROR_PIPE_BUSY = 231
)

var (
	kernel32                    = syscall.NewLazyDLL("kernel32.dll")
	procCreateFileW             = kernel32.NewProc("CreateFileW")
	procWaitNamedPipeW          = kernel32.NewProc("WaitNamedPipeW")
	procSetNamedPipeHandleState = kernel32.NewProc("SetNamedPipeHandleState")
)

// PipeClient represents a named pipe client
type PipeClient struct {
	handle syscall.Handle
	reader *bufio.Reader
	closed bool
}

// Connect connects to the trainer's named pipe
func Connect(timeout time.Duration) (*PipeClient, error) {
	pipePath, err := syscall.UTF16PtrFromString(PIPE_NAME)
	if err != nil {
		return nil, fmt.Errorf("failed to convert pipe name: %v", err)
	}

	deadline := time.Now().Add(timeout)

	for time.Now().Before(deadline) {
		// Try to open the pipe
		handle, err := createFile(pipePath)
		if err == nil {
			if err := setPipeReadModeMessage(handle); err != nil {
				syscall.CloseHandle(handle)
				return nil, fmt.Errorf("failed to configure pipe: %v", err)
			}

			// Success!
			file := os.NewFile(uintptr(handle), PIPE_NAME)
			return &PipeClient{
				handle: handle,
				reader: bufio.NewReader(file),
				closed: false,
			}, nil
		}

		// If pipe is busy, wait for it
		if errno, ok := err.(syscall.Errno); ok && errno == ERROR_PIPE_BUSY {
			// Wait up to 5 seconds for pipe to become available
			ret, _, _ := procWaitNamedPipeW.Call(
				uintptr(unsafe.Pointer(pipePath)),
				uintptr(5000), // 5 second timeout
			)

			if ret == 0 {
				return nil, fmt.Errorf("pipe is busy and WaitNamedPipe timed out")
			}

			// Try again
			continue
		}

		// Other error - wait a bit and retry
		time.Sleep(100 * time.Millisecond)
	}

	return nil, fmt.Errorf("failed to connect to pipe within timeout")
}

func setPipeReadModeMessage(handle syscall.Handle) error {
	var mode uint32 = PIPE_READMODE_MESSAGE
	ret, _, err := procSetNamedPipeHandleState.Call(
		uintptr(handle),
		uintptr(unsafe.Pointer(&mode)),
		0,
		0,
	)

	if ret == 0 {
		if errno, ok := err.(syscall.Errno); ok && errno != 0 {
			return errno
		}
		return fmt.Errorf("SetNamedPipeHandleState failed")
	}

	return nil
}

// createFile opens a handle to the named pipe
func createFile(name *uint16) (syscall.Handle, error) {
	handle, _, err := procCreateFileW.Call(
		uintptr(unsafe.Pointer(name)),
		uintptr(GENERIC_READ|GENERIC_WRITE),
		0, // No sharing
		0, // Default security
		uintptr(OPEN_EXISTING),
		0, // No attributes
		0, // No template file
	)

	if handle == uintptr(syscall.InvalidHandle) {
		return syscall.InvalidHandle, err
	}

	return syscall.Handle(handle), nil
}

// ReadMessage reads a message from the pipe
func (pc *PipeClient) ReadMessage() (*TrainerMessage, error) {
	if pc.closed {
		return nil, fmt.Errorf("pipe is closed")
	}

	// Read line (messages are newline-delimited)
	line, err := pc.reader.ReadBytes('\n')
	if err != nil {
		return nil, err
	}

	// Parse JSON
	return ParseMessage(line)
}

// Write writes raw bytes to the pipe (for future command support)
func (pc *PipeClient) Write(data []byte) error {
	if pc.closed {
		return fmt.Errorf("pipe is closed")
	}

	var written uint32
	err := syscall.WriteFile(pc.handle, data, &written, nil)
	return err
}

// Close closes the pipe connection
func (pc *PipeClient) Close() error {
	if pc.closed {
		return nil
	}

	pc.closed = true
	return syscall.CloseHandle(pc.handle)
}
