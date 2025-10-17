#include "pch.h"
#include <Windows.h>
#include <iostream>
#include <thread>
#include "trainer.h"
#include "memory.h"

// Console for debugging
void CreateDebugConsole() {
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    std::cout.clear();
    std::clog.clear();
    std::cerr.clear();
    std::cin.clear();
}

// Main thread that runs the trainer
DWORD WINAPI MainThread(LPVOID lpParameter) {
    // Create console for debugging (optional - can be removed for stealth)
    CreateDebugConsole();
    
    std::cout << "=== Assault Cube Trainer ===" << std::endl;
    std::cout << "Initializing..." << std::endl;
    
    // Get module base address
    uintptr_t moduleBase = (uintptr_t)GetModuleHandle(NULL);
    std::cout << "Module Base: 0x" << std::hex << moduleBase << std::dec << std::endl;
    
    // Initialize trainer
    Trainer* trainer = new Trainer(moduleBase);
    
    if (trainer->Initialize()) {
        std::cout << "Trainer initialized successfully!" << std::endl;
        std::cout << "\n=== Controls ===" << std::endl;
        std::cout << "F1  - Toggle God Mode" << std::endl;
        std::cout << "F2  - Toggle Infinite Ammo" << std::endl;
        std::cout << "F3  - Toggle No Recoil" << std::endl;
        std::cout << "F4  - Add 1000 Health" << std::endl;
        std::cout << "END - Exit Trainer" << std::endl;
        
        // Main loop
        trainer->Run();
        
        std::cout << "Trainer shutting down..." << std::endl;
    } else {
        std::cout << "Failed to initialize trainer!" << std::endl;
    }
    
    delete trainer;
    
    // Cleanup
    std::cout << "Press any key to exit..." << std::endl;
    std::cin.get();
    FreeConsole();
    
    // Eject DLL
    FreeLibraryAndExitThread((HMODULE)lpParameter, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        // Disable DLL_THREAD_ATTACH and DLL_THREAD_DETACH notifications
        DisableThreadLibraryCalls(hModule);
        
        // Create main thread
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
        break;
        
    case DLL_PROCESS_DETACH:
        // Cleanup code here if needed
        break;
    }
    return TRUE;
}
