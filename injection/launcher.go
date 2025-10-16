package injection

import (
	"fmt"
	"path/filepath"
	"syscall"
	"time"
	"unsafe"
)

var (
	kernel32DLL          = syscall.NewLazyDLL("kernel32.dll")
	user32DLL            = syscall.NewLazyDLL("user32.dll")
	procCreateProcessW   = kernel32DLL.NewProc("CreateProcessW")
	procResumeThread     = kernel32DLL.NewProc("ResumeThread")
	procWaitForInputIdle = user32DLL.NewProc("WaitForInputIdle")
	procTerminateProcess = kernel32DLL.NewProc("TerminateProcess")

	// Track active game process
	activeGameProcess syscall.Handle = 0
)

type STARTUPINFO struct {
	cb            uint32
	_             *uint16
	desktop       *uint16
	title         *uint16
	x             uint32
	y             uint32
	xSize         uint32
	ySize         uint32
	xCountChars   uint32
	yCountChars   uint32
	fillAttribute uint32
	flags         uint32
	showWindow    uint16
	_             uint16
	_             *byte
	stdInput      syscall.Handle
	stdOutput     syscall.Handle
	stdErr        syscall.Handle
}

type PROCESS_INFORMATION struct {
	process   syscall.Handle
	thread    syscall.Handle
	processId uint32
	threadId  uint32
}

const (
	CREATE_SUSPENDED = 0x00000004
)

// LaunchGame launches the game executable with proper working directory and command line
// Returns both PID and process handle (caller must close the handle!)
func LaunchGame(exePath string) (uint32, syscall.Handle, error) {
	// Assault Cube structure:
	//   AssaultCube/
	//   ├── bin_win32/
	//   │   └── ac_client.exe
	//   ├── packages/
	//   ├── config/
	//   └── assaultcube.bat (launches from here)

	// Get directories
	binDir := filepath.Dir(exePath)         // bin_win32
	assaultCubeRoot := filepath.Dir(binDir) // AssaultCube (working directory)

	// Build command line with FULL path to executable + args
	// The .bat uses relative path, but CreateProcessW needs absolute path OR we need to set lpApplicationName
	cmdLine := fmt.Sprintf(`"%s" --init`, exePath)

	// Convert to UTF16
	cmdLineUTF16, err := syscall.UTF16PtrFromString(cmdLine)
	if err != nil {
		return 0, 0, fmt.Errorf("failed to convert command line: %v", err)
	}

	assaultCubeRootUTF16, err := syscall.UTF16PtrFromString(assaultCubeRoot)
	if err != nil {
		return 0, 0, fmt.Errorf("failed to convert root dir: %v", err)
	}

	// Setup startup info
	var si STARTUPINFO
	si.cb = uint32(unsafe.Sizeof(si))

	// Setup process info
	var pi PROCESS_INFORMATION

	// Create process with AssaultCube root as working directory
	// Using full path in command line, but setting working dir to root
	ret, _, err := procCreateProcessW.Call(
		0,                                     // lpApplicationName (NULL, use cmdLine)
		uintptr(unsafe.Pointer(cmdLineUTF16)), // lpCommandLine (full path + args)
		0,                                     // lpProcessAttributes
		0,                                     // lpThreadAttributes
		0,                                     // bInheritHandles
		uintptr(0),                            // dwCreationFlags (normal)
		0,                                     // lpEnvironment
		uintptr(unsafe.Pointer(assaultCubeRootUTF16)), // lpCurrentDirectory (AssaultCube root!)
		uintptr(unsafe.Pointer(&si)),                  // lpStartupInfo
		uintptr(unsafe.Pointer(&pi)),                  // lpProcessInformation
	)

	if ret == 0 {
		return 0, 0, fmt.Errorf("CreateProcessW failed: %v", err)
	}

	// Close thread handle, but return process handle for WaitForInputIdle
	syscall.CloseHandle(pi.thread)

	return pi.processId, pi.process, nil
}

// LaunchAndInject launches the game and injects the DLL
func LaunchAndInject(exePath, dllPath string) error {
	// Launch the game with proper working directory
	pid, hProcess, err := LaunchGame(exePath)
	if err != nil {
		return fmt.Errorf("failed to launch game: %v", err)
	}

	// Store the process handle globally so we can terminate it later
	activeGameProcess = hProcess

	fmt.Printf("Game launched with PID: %d\n", pid)
	fmt.Println("Waiting for game to be ready for input...")

	// Wait for process to be ready (better than hardcoded sleep!)
	// WaitForInputIdle waits until process is waiting for user input
	// Timeout of 10 seconds (10000ms)
	ret, _, _ := procWaitForInputIdle.Call(
		uintptr(hProcess),
		uintptr(10000), // 10 second timeout
	)

	if ret == 0 {
		fmt.Println("Game is ready!")
	} else {
		// Timeout or error - fall back to a short sleep
		fmt.Println("WaitForInputIdle timed out, waiting 2 seconds...")
		time.Sleep(2 * time.Second)
	}

	// Inject the DLL
	fmt.Println("Attempting injection...")
	err = InjectDLL(pid, dllPath)
	if err != nil {
		// If injection fails, close the game
		TerminateActiveGame()
		return fmt.Errorf("injection failed (PID %d): %v", pid, err)
	}

	fmt.Println("Injection successful!")
	return nil
}

// TerminateActiveGame terminates the currently active game process
func TerminateActiveGame() {
	if activeGameProcess != 0 {
		procTerminateProcess.Call(uintptr(activeGameProcess), 0)
		syscall.CloseHandle(activeGameProcess)
		activeGameProcess = 0
	}
}

// IsGameRunning checks if the game process is still running
func IsGameRunning() bool {
	if activeGameProcess == 0 {
		return false
	}

	// Use WaitForSingleObject with 0 timeout to check if process is still running
	ret, _, _ := procWaitForSingleObject.Call(uintptr(activeGameProcess), 0)
	// WAIT_TIMEOUT (0x00000102) means process is still running
	return ret == 0x00000102
}
