#include <Windows.h>
#include <iostream>

int main() {
    const char* dllPath = "actrainer.dll";
    
    std::cout << "Attempting to load: " << dllPath << std::endl;
    
    HMODULE hModule = LoadLibraryA(dllPath);
    
    if (hModule == NULL) {
        DWORD error = GetLastError();
        std::cout << "LoadLibrary failed with error code: " << error << std::endl;
        
        // Print detailed error message
        LPVOID lpMsgBuf;
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&lpMsgBuf,
            0,
            NULL
        );
        
        std::cout << "Error message: " << (char*)lpMsgBuf << std::endl;
        LocalFree(lpMsgBuf);
        
        return 1;
    }
    
    std::cout << "DLL loaded successfully at address: 0x" << std::hex << hModule << std::endl;
    FreeLibrary(hModule);
    
    return 0;
}
