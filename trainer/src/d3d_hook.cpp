#include "pch.h"
#include "d3d_hook.h"

#include <d3d9.h>
#include <mutex>

#include "MinHook.h"

namespace {

using PresentFn = HRESULT(APIENTRY*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);

constexpr size_t kPresentVTableIndex = 17;

std::mutex g_hookMutex;
PresentFn g_originalPresent = nullptr;
void* g_presentTarget = nullptr;
bool g_hooksInstalled = false;

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

HRESULT APIENTRY PresentDetour(IDirect3DDevice9* device, const RECT* srcRect, const RECT* dstRect,
                               HWND overrideWindow, const RGNDATA* dirtyRegion) {
    // TODO: Overlay rendering will happen here.
    if (g_originalPresent) {
        return g_originalPresent(device, srcRect, dstRect, overrideWindow, dirtyRegion);
    }
    return D3DERR_INVALIDCALL;
}

} // namespace

bool InstallHooks(HWND gameWindow) {
    std::lock_guard<std::mutex> lock(g_hookMutex);

    if (g_hooksInstalled) {
        return true;
    }

    DeviceResources resources;
    if (!CreateTemporaryDevice(gameWindow, resources) || !resources.device) {
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(resources.device);
    if (!vtable) {
        return false;
    }

    void* presentTarget = vtable[kPresentVTableIndex];
    if (!presentTarget) {
        return false;
    }

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        return false;
    }

    status = MH_CreateHook(presentTarget, reinterpret_cast<LPVOID>(&PresentDetour),
                           reinterpret_cast<LPVOID*>(&g_originalPresent));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        MH_Uninitialize();
        g_originalPresent = nullptr;
        return false;
    }

    status = MH_EnableHook(presentTarget);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(presentTarget);
        MH_Uninitialize();
        g_originalPresent = nullptr;
        return false;
    }

    g_presentTarget = presentTarget;
    g_hooksInstalled = true;
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

    MH_Uninitialize();

    g_presentTarget = nullptr;
    g_originalPresent = nullptr;
    g_hooksInstalled = false;
}
