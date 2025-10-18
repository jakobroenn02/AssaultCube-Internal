#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        MessageBoxA(NULL, "Minimal DLL Loaded!", "Test", MB_OK);
    }
    return TRUE;
}
