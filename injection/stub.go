//go:build !windows
// +build !windows

package injection

import "fmt"

// LaunchAndInject is not supported on non-Windows platforms.
func LaunchAndInject(exePath, dllPath string) error {
	return fmt.Errorf("trainer injection is only supported on Windows")
}

// TerminateActiveGame is a no-op on non-Windows platforms.
func TerminateActiveGame() {}

// IsGameRunning always reports false on non-Windows platforms.
func IsGameRunning() bool {
	return false
}
