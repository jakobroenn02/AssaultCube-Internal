#include <Windows.h>

// Ultra minimal DLL - no C++ runtime dependencies
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        // Just show a message box - no file I/O, no C++ runtime
        MessageBoxA(NULL, "Minimal DLL Loaded Successfully!", "Test", MB_OK);
    }
    return TRUE;
}
