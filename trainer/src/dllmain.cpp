#include "pch.h"
#include <Windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include "trainer.h"
#include "memory.h"

namespace {
HMODULE g_moduleHandle = nullptr;
Trainer* g_trainerInstance = nullptr;
HHOOK g_getMessageHook = nullptr;
std::atomic<bool> g_inputCaptureEnabled{ false };

void UpdateInputCaptureState(bool enabled) {
    g_inputCaptureEnabled.store(enabled);

    if (g_trainerInstance) {
        g_trainerInstance->SetOverlayMenuVisible(enabled);
    }

    std::cout << "Overlay input capture " << (enabled ? "enabled" : "disabled") << std::endl;
}

LRESULT CALLBACK GetMessageHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && g_trainerInstance && lParam) {
        MSG* msg = reinterpret_cast<MSG*>(lParam);
        if (msg) {
            bool handled = false;

            if (msg->message == WM_KEYDOWN && msg->wParam == VK_INSERT) {
                bool initialPress = ((msg->lParam & (1 << 30)) == 0);
                if (initialPress) {
                    bool enable = !g_inputCaptureEnabled.load();
                    UpdateInputCaptureState(enable);
                }
                handled = true;
            }

            if (g_inputCaptureEnabled.load()) {
                handled = g_trainerInstance->ProcessMessage(*msg, true) || handled;
            }

            if (handled) {
                msg->message = WM_NULL;
                msg->wParam = 0;
                msg->lParam = 0;
            }
        }
    }

    return CallNextHookEx(g_getMessageHook, code, wParam, lParam);
}

bool InstallInputHook(HWND gameWindow) {
    if (!gameWindow) {
        return false;
    }

    DWORD threadId = GetWindowThreadProcessId(gameWindow, nullptr);
    if (threadId == 0) {
        return false;
    }

    g_getMessageHook = SetWindowsHookEx(WH_GETMESSAGE, GetMessageHookProc, g_moduleHandle, threadId);
    if (!g_getMessageHook) {
        return false;
    }

    g_inputCaptureEnabled.store(false);

    if (g_trainerInstance) {
        g_trainerInstance->SetMessagePumpInputEnabled(true);
    }

    std::cout << "Windows message hook installed for overlay input." << std::endl;
    std::cout << "Press INSERT to enable overlay input capture." << std::endl;
    return true;
}

void RemoveInputHook() {
    if (g_getMessageHook) {
        UnhookWindowsHookEx(g_getMessageHook);
        g_getMessageHook = nullptr;
    }

    if (g_trainerInstance) {
        g_trainerInstance->SetMessagePumpInputEnabled(false);
        g_trainerInstance->SetOverlayMenuVisible(false);
    }

    g_inputCaptureEnabled.store(false);
}
} // namespace

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
    g_trainerInstance = trainer;

    if (trainer->Initialize()) {
        std::cout << "Trainer initialized successfully!" << std::endl;
        std::cout << "\n=== Controls ===" << std::endl;
        std::cout << "F1  - Toggle God Mode" << std::endl;
        std::cout << "F2  - Toggle Infinite Ammo" << std::endl;
        std::cout << "F3  - Toggle No Recoil" << std::endl;
        std::cout << "F4  - Add 1000 Health" << std::endl;
        std::cout << "END - Exit Trainer" << std::endl;

        if (!InstallInputHook(trainer->GetGameWindowHandle())) {
            std::cout << "WARNING: Failed to install input hook. Falling back to polling input." << std::endl;
        }

        // Main loop
        trainer->Run();

        std::cout << "Trainer shutting down..." << std::endl;
    } else {
        std::cout << "Failed to initialize trainer!" << std::endl;
    }

    RemoveInputHook();

    delete trainer;
    g_trainerInstance = nullptr;

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
        g_moduleHandle = hModule;
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
