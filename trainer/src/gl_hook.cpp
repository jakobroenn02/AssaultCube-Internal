#include "pch.h"
#include "gl_hook.h"

#include <mutex>
#include <iostream>

#include "MinHook.h"
#include "ui.h"
#include "trainer.h"

namespace {
using SwapBuffersFn = BOOL(WINAPI*)(HDC);

std::mutex g_hookMutex;
SwapBuffersFn g_originalSwapBuffers = nullptr;
void* g_swapBuffersTarget = nullptr;
bool g_hooksInstalled = false;
Trainer* g_trainer = nullptr;
UIRenderer* g_uiRenderer = nullptr;

BOOL WINAPI SwapBuffersDetour(HDC hdc) {
    if (g_uiRenderer && g_trainer) {
        g_uiRenderer->Render(*g_trainer);
    }

    if (g_originalSwapBuffers) {
        return g_originalSwapBuffers(hdc);
    }

    return ::SwapBuffers(hdc);
}

void ResetState() {
    g_originalSwapBuffers = nullptr;
    g_swapBuffersTarget = nullptr;
    g_hooksInstalled = false;
    g_trainer = nullptr;
    g_uiRenderer = nullptr;
}

} // namespace

bool InstallHooks(HWND gameWindow, Trainer* trainer, UIRenderer* renderer) {
    std::lock_guard<std::mutex> lock(g_hookMutex);

    std::cout << "[GL Hook] InstallHooks called. Window: " << gameWindow
              << ", Trainer: " << trainer << ", Renderer: " << renderer << std::endl;

    if (g_hooksInstalled) {
        std::cout << "[GL Hook] Hooks already installed, updating pointers." << std::endl;
        g_trainer = trainer;
        g_uiRenderer = renderer;
        return true;
    }

    HMODULE openglModule = GetModuleHandleA("opengl32.dll");
    if (!openglModule) {
        openglModule = LoadLibraryA("opengl32.dll");
    }

    if (!openglModule) {
        std::cout << "[GL Hook] ERROR: Failed to load opengl32.dll" << std::endl;
        return false;
    }

    void* target = reinterpret_cast<void*>(GetProcAddress(openglModule, "wglSwapBuffers"));
    if (!target) {
        std::cout << "[GL Hook] ERROR: Failed to resolve wglSwapBuffers" << std::endl;
        return false;
    }

    std::cout << "[GL Hook] wglSwapBuffers located at: " << target << std::endl;

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        std::cout << "[GL Hook] ERROR: MH_Initialize failed with status: " << status << std::endl;
        return false;
    }

    status = MH_CreateHook(target, reinterpret_cast<LPVOID>(&SwapBuffersDetour),
                           reinterpret_cast<LPVOID*>(&g_originalSwapBuffers));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        std::cout << "[GL Hook] ERROR: MH_CreateHook failed with status: " << status << std::endl;
        MH_Uninitialize();
        ResetState();
        return false;
    }

    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        std::cout << "[GL Hook] ERROR: MH_EnableHook failed with status: " << status << std::endl;
        MH_RemoveHook(target);
        MH_Uninitialize();
        ResetState();
        return false;
    }

    g_swapBuffersTarget = target;
    g_hooksInstalled = true;
    g_trainer = trainer;
    g_uiRenderer = renderer;

    std::cout << "[GL Hook] Hook installation complete. OpenGL overlay active." << std::endl;
    return true;
}

void RemoveHooks() {
    std::lock_guard<std::mutex> lock(g_hookMutex);

    if (!g_hooksInstalled) {
        return;
    }

    if (g_swapBuffersTarget) {
        MH_DisableHook(g_swapBuffersTarget);
        MH_RemoveHook(g_swapBuffersTarget);
    }

    MH_Uninitialize();
    ResetState();
}
