package injection

import (
	"fmt"
	"path/filepath"
	"syscall"
	"time"
	"unsafe"
)

var (
	kernel32DLL        = syscall.NewLazyDLL("kernel32.dll")
	procCreateProcessW = kernel32DLL.NewProc("CreateProcessW")
	procResumeThread   = kernel32DLL.NewProc("ResumeThread")
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
func LaunchGame(exePath string) (uint32, error) {
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
		return 0, fmt.Errorf("failed to convert command line: %v", err)
	}

	assaultCubeRootUTF16, err := syscall.UTF16PtrFromString(assaultCubeRoot)
	if err != nil {
		return 0, fmt.Errorf("failed to convert root dir: %v", err)
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
		return 0, fmt.Errorf("CreateProcessW failed: %v", err)
	}

	// Close handles we don't need
	syscall.CloseHandle(pi.process)
	syscall.CloseHandle(pi.thread)

	return pi.processId, nil
}

// LaunchAndInject launches the game and injects the DLL
func LaunchAndInject(exePath, dllPath string) error {
	// Launch the game with proper working directory
	pid, err := LaunchGame(exePath)
	if err != nil {
		return fmt.Errorf("failed to launch game: %v", err)
	}

	// Give the process MORE time to initialize before injecting
	// Assault Cube needs time to load modules (kernel32, user32, etc.)
	// Injecting too early can cause the process to not have LoadLibraryA ready
	fmt.Printf("Game launched with PID: %d\n", pid)
	fmt.Println("Waiting for game to initialize...")
	time.Sleep(3 * time.Second)

	// Inject the DLL
	fmt.Println("Attempting injection...")
	err = InjectDLL(pid, dllPath)
	if err != nil {
		return fmt.Errorf("injection failed (PID %d): %v", pid, err)
	}

	fmt.Println("Injection successful!")
	return nil
}
