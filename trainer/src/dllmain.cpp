#include "pch.h"
#include <Windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include "trainer.h"
#include "ui.h"
#include "memory.h"
#include "imgui_impl_win32.h"

// Forward declare ImGui's Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
                // Pass messages to ImGui first so it can handle mouse input
                ImGui_ImplWin32_WndProcHandler(msg->hwnd, msg->message, msg->wParam, msg->lParam);
                
                // Block keyboard and RAW MOUSE INPUT when menu is visible
                // WM_INPUT is used by games for raw mouse movement (camera control)
                bool isKeyboardMessage = (msg->message >= WM_KEYFIRST && msg->message <= WM_KEYLAST);
                bool isRawInput = (msg->message == WM_INPUT);
                
                if ((isKeyboardMessage && msg->wParam != VK_ESCAPE) || isRawInput) {
                    // Block keyboard (except ESC) and raw input
                    handled = true;
                }
                
                // Let trainer process for ESC handling
                g_trainerInstance->ProcessMessage(*msg, true);
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

        // Set up callback to sync input capture state with UI visibility
        if (trainer->GetUIRenderer()) {
            // Save/restore game mouse lock bytes when menu visibility changes.
            // Addresses provided by user (ac_offsets.md):
            // 0x00593F19 - Mouse Lock State (0 = unlocked, 1 = locked)
            // 0x00593F13 - Mouse Capture State (0 = not captured, 1 = captured)
            // 0x0056D925 - Relative Mouse Config (0 = disabled, 1 = enabled)
            static const uintptr_t kMouseLockAddr = 0x00593F19;
            static const uintptr_t kMouseCaptureAddr = 0x00593F13;
            static const uintptr_t kRelMouseAddr = 0x0056D925;
            static uint8_t savedLock = 0;
            static uint8_t savedCapture = 0;
            static uint8_t savedRel = 0;
            static bool savedValid = false;

            trainer->GetUIRenderer()->SetMenuVisibilityCallback([=](bool visible) mutable {
                // Sync input capture flag used by our hook
                g_inputCaptureEnabled.store(visible);

                // When showing menu, clear game's mouse lock/capture so the game stops fighting our cursor
                if (visible) {
                    if (!savedValid) {
                        // Read current values and save them
                        savedLock = Memory::Read<uint8_t>(kMouseLockAddr);
                        savedCapture = Memory::Read<uint8_t>(kMouseCaptureAddr);
                        savedRel = Memory::Read<uint8_t>(kRelMouseAddr);
                        savedValid = true;
                    }

                    // Write zeros to unlock/capture flags
                    Memory::Write<uint8_t>(kMouseLockAddr, 0);
                    Memory::Write<uint8_t>(kMouseCaptureAddr, 0);
                    Memory::Write<uint8_t>(kRelMouseAddr, 0);
                    std::cout << "[UI] Cleared game mouse lock/capture flags while menu is open" << std::endl;
                } else {
                    // Restore saved values when hiding menu
                    if (savedValid) {
                        Memory::Write<uint8_t>(kMouseLockAddr, savedLock);
                        Memory::Write<uint8_t>(kMouseCaptureAddr, savedCapture);
                        Memory::Write<uint8_t>(kRelMouseAddr, savedRel);
                        savedValid = false;
                        std::cout << "[UI] Restored game mouse lock/capture flags" << std::endl;
                    }
                }

                std::cout << "Input capture state synced: " << (visible ? "enabled" : "disabled") << std::endl;
            });
        }

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
