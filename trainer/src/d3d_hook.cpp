#include "pch.h"
#include "d3d_hook.h"

#include <d3d9.h>
#include <mutex>

#include "MinHook.h"
#include "ui.h"

namespace {

using PresentFn = HRESULT(APIENTRY*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);

constexpr size_t kPresentVTableIndex = 17;

std::mutex g_hookMutex;
PresentFn g_originalPresent = nullptr;
void* g_presentTarget = nullptr;
void** g_presentVtableEntry = nullptr;
bool g_hooksInstalled = false;
Trainer* g_trainer = nullptr;
UIRenderer* g_uiRenderer = nullptr;

struct DeviceResources {
    IDirect3D9* d3d = nullptr;
    IDirect3DDevice9* device = nullptr;
    HWND tempWindow = nullptr;

    ~DeviceResources() {
        if (device) {
            device->Release();
            device = nullptr;
        }
        if (d3d) {
            d3d->Release();
            d3d = nullptr;
        }
        if (tempWindow && IsWindow(tempWindow)) {
            DestroyWindow(tempWindow);
            tempWindow = nullptr;
        }
    }
};

LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND CreateTemporaryWindowHandle() {
    const char kClassName[] = "ACT_MinHookDummy";
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA) };
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = kClassName;

    if (!RegisterClassExA(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            return nullptr;
        }
    }

    return CreateWindowExA(0, kClassName, "", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,
                           nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
}

bool CreateTemporaryDevice(HWND gameWindow, DeviceResources& resources) {
    resources.d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!resources.d3d) {
        return false;
    }

    HWND targetWindow = gameWindow;
    if (!targetWindow || !IsWindow(targetWindow)) {
        targetWindow = CreateTemporaryWindowHandle();
        resources.tempWindow = targetWindow;
    }

    if (!targetWindow) {
        return false;
    }

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferWidth = 1;
    d3dpp.BackBufferHeight = 1;
    d3dpp.hDeviceWindow = targetWindow;

    HRESULT hr = resources.d3d->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        targetWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp,
        &resources.device);

    if (FAILED(hr)) {
        // Try reference device as a fallback.
        hr = resources.d3d->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_REF,
            targetWindow,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &d3dpp,
            &resources.device);
    }

    return SUCCEEDED(hr);
}

bool TryResetDevice(IDirect3DDevice9* device) {
    if (!device) {
        return false;
    }

    IDirect3DSwapChain9* swapChain = nullptr;
    HRESULT hr = device->GetSwapChain(0, &swapChain);
    if (FAILED(hr) || !swapChain) {
        if (swapChain) {
            swapChain->Release();
        }
        return false;
    }

    D3DPRESENT_PARAMETERS params = {};
    hr = swapChain->GetPresentParameters(&params);
    swapChain->Release();
    if (FAILED(hr)) {
        return false;
    }

    hr = device->Reset(&params);
    return SUCCEEDED(hr);
}

HRESULT APIENTRY PresentDetour(IDirect3DDevice9* device, const RECT* srcRect, const RECT* dstRect,
                               HWND overrideWindow, const RGNDATA* dirtyRegion) {
    // Note: No console output here - it's called every frame and can cause crashes

    if (!device) {
        return g_originalPresent ? g_originalPresent(device, srcRect, dstRect, overrideWindow, dirtyRegion)
                                 : D3DERR_INVALIDCALL;
    }

    if (!g_presentVtableEntry) {
        void** vtable = *reinterpret_cast<void***>(device);
        if (vtable) {
            g_presentVtableEntry = &vtable[kPresentVTableIndex];
        }
    }

    bool deviceReadyForRender = true;
    if (g_uiRenderer && g_trainer) {
        HRESULT coopResult = device->TestCooperativeLevel();
        if (coopResult == D3DERR_DEVICELOST) {
            g_uiRenderer->OnDeviceLost();
            deviceReadyForRender = false;
        } else if (coopResult == D3DERR_DEVICENOTRESET) {
            g_uiRenderer->OnDeviceLost();
            if (TryResetDevice(device)) {
                g_uiRenderer->OnDeviceReset(device);
                deviceReadyForRender = true;
            } else {
                deviceReadyForRender = false;
            }
        } else if (FAILED(coopResult)) {
            deviceReadyForRender = false;
        }

        if (deviceReadyForRender) {
            g_uiRenderer->Render(device, *g_trainer);
        }
    }

    HRESULT result = g_originalPresent ? g_originalPresent(device, srcRect, dstRect, overrideWindow, dirtyRegion)
                                       : D3DERR_INVALIDCALL;

    if (g_uiRenderer && (result == D3DERR_DEVICELOST || result == D3DERR_DEVICENOTRESET)) {
        g_uiRenderer->OnDeviceLost();
    }

    return result;
}

} // namespace

bool InstallHooks(HWND gameWindow, Trainer* trainer, UIRenderer* renderer) {
    std::lock_guard<std::mutex> lock(g_hookMutex);

    std::cout << "[D3D Hook] InstallHooks called. Window: " << gameWindow 
              << ", Trainer: " << trainer << ", Renderer: " << renderer << std::endl;

    if (g_hooksInstalled) {
        std::cout << "[D3D Hook] Hooks already installed, updating pointers." << std::endl;
        g_trainer = trainer;
        g_uiRenderer = renderer;
        return true;
    }

    std::cout << "[D3D Hook] Creating temporary D3D device..." << std::endl;
    DeviceResources resources;
    if (!CreateTemporaryDevice(gameWindow, resources) || !resources.device) {
        std::cout << "[D3D Hook] ERROR: Failed to create temporary device!" << std::endl;
        return false;
    }
    std::cout << "[D3D Hook] Temporary device created: " << resources.device << std::endl;

    void** vtable = *reinterpret_cast<void***>(resources.device);
    if (!vtable) {
        std::cout << "[D3D Hook] ERROR: Failed to get device vtable!" << std::endl;
        return false;
    }
    std::cout << "[D3D Hook] Device vtable: " << vtable << std::endl;

    void* presentTarget = vtable[kPresentVTableIndex];
    if (!presentTarget) {
        std::cout << "[D3D Hook] ERROR: Failed to get Present function pointer!" << std::endl;
        return false;
    }
    std::cout << "[D3D Hook] Present function at: " << presentTarget << std::endl;

    std::cout << "[D3D Hook] Initializing MinHook..." << std::endl;
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        std::cout << "[D3D Hook] ERROR: MH_Initialize failed with status: " << status << std::endl;
        return false;
    }
    std::cout << "[D3D Hook] MinHook initialized." << std::endl;

    std::cout << "[D3D Hook] Creating hook for Present..." << std::endl;
    status = MH_CreateHook(presentTarget, reinterpret_cast<LPVOID>(&PresentDetour),
                           reinterpret_cast<LPVOID*>(&g_originalPresent));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        std::cout << "[D3D Hook] ERROR: MH_CreateHook failed with status: " << status << std::endl;
        MH_Uninitialize();
        g_originalPresent = nullptr;
        return false;
    }
    std::cout << "[D3D Hook] Hook created. Original Present: " << g_originalPresent << std::endl;

    std::cout << "[D3D Hook] Enabling hook..." << std::endl;
    status = MH_EnableHook(presentTarget);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        std::cout << "[D3D Hook] ERROR: MH_EnableHook failed with status: " << status << std::endl;
        MH_RemoveHook(presentTarget);
        MH_Uninitialize();
        g_originalPresent = nullptr;
        return false;
    }

    g_presentTarget = presentTarget;
    g_hooksInstalled = true;
    g_trainer = trainer;
    g_uiRenderer = renderer;
    
    std::cout << "[D3D Hook] Hook installation complete! Present function is now hooked." << std::endl;
    return true;
}

void RemoveHooks() {
    std::lock_guard<std::mutex> lock(g_hookMutex);

    if (!g_hooksInstalled) {
        return;
    }

    if (g_presentTarget) {
        MH_DisableHook(g_presentTarget);
        MH_RemoveHook(g_presentTarget);
    }

    if (g_presentVtableEntry && g_originalPresent) {
        DWORD oldProtect = 0;
        if (VirtualProtect(g_presentVtableEntry, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            *g_presentVtableEntry = reinterpret_cast<void*>(g_originalPresent);
            DWORD unused = 0;
            VirtualProtect(g_presentVtableEntry, sizeof(void*), oldProtect, &unused);
        }
    }

    g_presentVtableEntry = nullptr;

    MH_Uninitialize();

    g_presentTarget = nullptr;
    g_originalPresent = nullptr;
    g_hooksInstalled = false;
    g_trainer = nullptr;
    g_uiRenderer = nullptr;
}
