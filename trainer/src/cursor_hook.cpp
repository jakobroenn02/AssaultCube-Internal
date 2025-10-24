#include "pch.h"
#include "cursor_hook.h"
#include <MinHook.h>
#include <iostream>

namespace {
    // Original ClipCursor function pointer
    typedef BOOL(WINAPI* ClipCursor_t)(CONST RECT* lpRect);
    ClipCursor_t g_originalClipCursor = nullptr;
    
    // Flag to control whether we allow cursor clipping
    bool g_allowCursorClip = true;
    
    // Our hooked ClipCursor function
    BOOL WINAPI HookedClipCursor(CONST RECT* lpRect) {
        // If we're blocking cursor clipping and the game is trying to clip the cursor
        if (!g_allowCursorClip && lpRect != nullptr) {
            std::cout << "[CursorHook] Blocked game's ClipCursor call" << std::endl;
            // Instead of clipping, just unclip
            return g_originalClipCursor(nullptr);
        }
        
        // Otherwise, allow the call through
        return g_originalClipCursor(lpRect);
    }
}

bool InstallCursorHook() {
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        std::cout << "[CursorHook] Failed to initialize MinHook" << std::endl;
        return false;
    }
    
    // Hook ClipCursor from User32.dll
    if (MH_CreateHookApi(L"user32", "ClipCursor", &HookedClipCursor, reinterpret_cast<LPVOID*>(&g_originalClipCursor)) != MH_OK) {
        std::cout << "[CursorHook] Failed to create ClipCursor hook" << std::endl;
        return false;
    }
    
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        std::cout << "[CursorHook] Failed to enable ClipCursor hook" << std::endl;
        return false;
    }
    
    std::cout << "[CursorHook] Successfully hooked ClipCursor!" << std::endl;
    return true;
}

void RemoveCursorHook() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

void SetCursorClipBlocked(bool blocked) {
    g_allowCursorClip = !blocked;
    
    // If we're unblocking, make sure cursor is actually free
    if (!blocked) {
        if (g_originalClipCursor) {
            g_originalClipCursor(nullptr);
        }
    }
    
    std::cout << "[CursorHook] Cursor clipping " << (blocked ? "BLOCKED" : "ALLOWED") << std::endl;
}
