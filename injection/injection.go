//go:build windows
// +build windows

package injection

import (
	"errors"
	"fmt"
	"syscall"
	"unsafe"
)

var (
	kernel32                = syscall.NewLazyDLL("kernel32.dll")
	procOpenProcess         = kernel32.NewProc("OpenProcess")
	procVirtualAllocEx      = kernel32.NewProc("VirtualAllocEx")
	procWriteProcessMemory  = kernel32.NewProc("WriteProcessMemory")
	procCreateRemoteThread  = kernel32.NewProc("CreateRemoteThread")
	procGetProcAddress      = kernel32.NewProc("GetProcAddress")
	procGetModuleHandleA    = kernel32.NewProc("GetModuleHandleA")
	procCloseHandle         = kernel32.NewProc("CloseHandle")
	procWaitForSingleObject = kernel32.NewProc("WaitForSingleObject")
	procGetExitCodeThread   = kernel32.NewProc("GetExitCodeThread")
)

const (
	PROCESS_ALL_ACCESS = 0x1F0FFF
	MEM_COMMIT         = 0x1000
	MEM_RESERVE        = 0x2000
	PAGE_READWRITE     = 0x04
	INFINITE           = 0xFFFFFFFF
)

// InjectDLL injects a DLL into a target process
func InjectDLL(processID uint32, dllPath string) error {
	// 1. Open target process
	hProcess, err := openProcess(PROCESS_ALL_ACCESS, false, processID)
	if err != nil {
		return fmt.Errorf("failed to open process: %v", err)
	}
	defer closeHandle(hProcess)

	// 2. Allocate memory in target process for DLL path
	dllPathBytes := append([]byte(dllPath), 0) // Null-terminated string
	remoteMem, err := virtualAllocEx(hProcess, 0, len(dllPathBytes), MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
	if err != nil {
		return fmt.Errorf("failed to allocate memory: %v", err)
	}

	// 3. Write DLL path to allocated memory
	err = writeProcessMemory(hProcess, remoteMem, dllPathBytes)
	if err != nil {
		return fmt.Errorf("failed to write DLL path: %v", err)
	}

	// 4. Get address of LoadLibraryA
	hKernel32, err := getModuleHandle("kernel32.dll")
	if err != nil {
		return fmt.Errorf("failed to get kernel32 handle: %v", err)
	}

	loadLibraryAddr, err := getProcAddress(hKernel32, "LoadLibraryA")
	if err != nil {
		return fmt.Errorf("failed to get LoadLibraryA address: %v", err)
	}

	// 5. Create remote thread to call LoadLibraryA
	hThread, err := createRemoteThread(hProcess, 0, 0, loadLibraryAddr, remoteMem, 0, nil)
	if err != nil {
		return fmt.Errorf("failed to create remote thread: %v", err)
	}
	defer closeHandle(hThread)

	// 6. Wait for the remote thread to finish
	waitForSingleObject(hThread, INFINITE)

	// 7. Get thread exit code to verify LoadLibrary succeeded
	var exitCode uint32
	ret, _, _ := procGetExitCodeThread.Call(uintptr(hThread), uintptr(unsafe.Pointer(&exitCode)))
	if ret == 0 {
		return fmt.Errorf("failed to get thread exit code")
	}

	// Exit code is the return value of LoadLibraryA (HMODULE handle)
	// If it's 0, the DLL failed to load
	if exitCode == 0 {
		// Try to get more info - check if file exists and is readable
		if _, err := syscall.UTF16PtrFromString(dllPath); err != nil {
			return fmt.Errorf("DLL path contains invalid characters: %s", dllPath)
		}

		return dllLoadError(dllPath)
	}

	return nil
}

// dllLoadError returns a detailed error when LoadLibraryA fails to load the DLL.
func dllLoadError(dllPath string) error {
	return fmt.Errorf(
		"LoadLibraryA returned NULL - DLL failed to load. Possible causes:\n"+
			"  1. Architecture mismatch (DLL is 32-bit, game might be 64-bit or vice versa)\n"+
			"  2. Missing dependencies (use Dependency Walker to check)\n"+
			"  3. DLL is blocked by Windows (right-click DLL -> Properties -> Unblock)\n"+
			"  4. Antivirus blocking the DLL\n"+
			"  DLL Path: %s", dllPath)
}

func openProcess(desiredAccess uint32, inheritHandle bool, processID uint32) (syscall.Handle, error) {
	inherit := 0
	if inheritHandle {
		inherit = 1
	}

	ret, _, err := procOpenProcess.Call(
		uintptr(desiredAccess),
		uintptr(inherit),
		uintptr(processID),
	)

	if ret == 0 {
		return 0, err
	}

	return syscall.Handle(ret), nil
}

func virtualAllocEx(hProcess syscall.Handle, address uintptr, size int, allocationType, protect uint32) (uintptr, error) {
	ret, _, err := procVirtualAllocEx.Call(
		uintptr(hProcess),
		address,
		uintptr(size),
		uintptr(allocationType),
		uintptr(protect),
	)

	if ret == 0 {
		return 0, err
	}

	return ret, nil
}

func writeProcessMemory(hProcess syscall.Handle, baseAddress uintptr, buffer []byte) error {
	var bytesWritten uintptr

	ret, _, err := procWriteProcessMemory.Call(
		uintptr(hProcess),
		baseAddress,
		uintptr(unsafe.Pointer(&buffer[0])),
		uintptr(len(buffer)),
		uintptr(unsafe.Pointer(&bytesWritten)),
	)

	if ret == 0 {
		return err
	}

	if int(bytesWritten) != len(buffer) {
		return errors.New("failed to write all bytes")
	}

	return nil
}

func getModuleHandle(moduleName string) (syscall.Handle, error) {
	moduleNamePtr, err := syscall.BytePtrFromString(moduleName)
	if err != nil {
		return 0, err
	}

	ret, _, err := procGetModuleHandleA.Call(uintptr(unsafe.Pointer(moduleNamePtr)))
	if ret == 0 {
		return 0, err
	}

	return syscall.Handle(ret), nil
}

func getProcAddress(hModule syscall.Handle, procName string) (uintptr, error) {
	procNamePtr, err := syscall.BytePtrFromString(procName)
	if err != nil {
		return 0, err
	}

	ret, _, err := procGetProcAddress.Call(
		uintptr(hModule),
		uintptr(unsafe.Pointer(procNamePtr)),
	)

	if ret == 0 {
		return 0, err
	}

	return ret, nil
}

func createRemoteThread(hProcess syscall.Handle, lpThreadAttributes uintptr, dwStackSize uint32,
	lpStartAddress, lpParameter uintptr, dwCreationFlags uint32, lpThreadId *uint32) (syscall.Handle, error) {

	ret, _, err := procCreateRemoteThread.Call(
		uintptr(hProcess),
		lpThreadAttributes,
		uintptr(dwStackSize),
		lpStartAddress,
		lpParameter,
		uintptr(dwCreationFlags),
		uintptr(unsafe.Pointer(lpThreadId)),
	)

	if ret == 0 {
		return 0, err
	}

	return syscall.Handle(ret), nil
}

func waitForSingleObject(hHandle syscall.Handle, dwMilliseconds uint32) error {
	ret, _, err := procWaitForSingleObject.Call(
		uintptr(hHandle),
		uintptr(dwMilliseconds),
	)

	if ret == 0xFFFFFFFF {
		return err
	}

	return nil
}

func closeHandle(hObject syscall.Handle) error {
	ret, _, err := procCloseHandle.Call(uintptr(hObject))
	if ret == 0 {
		return err
	}
	return nil
}
