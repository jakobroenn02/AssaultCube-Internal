#include "pch.h"
#include "ui.h"
#include <cstdio>
#include <thread>

// Window class name for overlay
static const char* OVERLAY_CLASS_NAME = "ACTrainerOverlay";

// Window procedure for overlay window
LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            // Painting is handled manually in Render()
            ValidateRect(hwnd, NULL);
            return 0;
        }
        case WM_DESTROY:
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

UIRenderer::UIRenderer()
    : gameWindow(NULL),
      overlayWindow(NULL),
      deviceContext(NULL),
      memoryDC(NULL),
      memoryBitmap(NULL),
      oldBitmap(NULL),
      backgroundBrush(NULL),
      activeFeatureBrush(NULL),
      inactiveFeatureBrush(NULL),
      buttonBrush(NULL),
      buttonHoverBrush(NULL),
      headerFont(NULL),
      normalFont(NULL),
      smallFont(NULL),
      windowWidth(800),
      windowHeight(600),
      panelX(20),
      panelY(20),
      panelWidth(350),
      panelHeight(400),
      selectedIndex(0),
      menuVisible(false) {
}

UIRenderer::~UIRenderer() {
    Shutdown();
}

bool UIRenderer::Initialize(HWND targetWindow) {
    if (!targetWindow) {
        return false;
    }
    
    gameWindow = targetWindow;
    
    // Get window dimensions
    RECT clientRect;
    if (GetClientRect(gameWindow, &clientRect)) {
        windowWidth = clientRect.right - clientRect.left;
        windowHeight = clientRect.bottom - clientRect.top;
    }
    
    // Register overlay window class
    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = OVERLAY_CLASS_NAME;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    
    if (!RegisterClassExA(&wc)) {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }
    
    // Get game window position on screen
    RECT gameRect;
    GetWindowRect(gameWindow, &gameRect);
    
    // Create transparent overlay window that follows the game window
    overlayWindow = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        OVERLAY_CLASS_NAME,
        "AC Trainer Overlay",
        WS_POPUP,
        gameRect.left, gameRect.top,
        windowWidth, windowHeight,
        NULL, NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!overlayWindow) {
        return false;
    }
    
    // Make overlay click-through and 90% transparent background
    SetLayeredWindowAttributes(overlayWindow, RGB(0, 0, 0), 0, LWA_COLORKEY);
    
    // Show the overlay window
    ShowWindow(overlayWindow, SW_SHOW);
    
    // Get DC for overlay window
    deviceContext = GetDC(overlayWindow);
    if (!deviceContext) {
        return false;
    }
    
    // Create memory DC for double buffering
    memoryDC = CreateCompatibleDC(deviceContext);
    memoryBitmap = CreateCompatibleBitmap(deviceContext, windowWidth, windowHeight);
    oldBitmap = (HBITMAP)SelectObject(memoryDC, memoryBitmap);
    
    // Create brushes
    backgroundBrush = CreateSolidBrush(UIColors::BACKGROUND);
    activeFeatureBrush = CreateSolidBrush(UIColors::ACTIVE);
    inactiveFeatureBrush = CreateSolidBrush(UIColors::INACTIVE);
    buttonBrush = CreateSolidBrush(UIColors::BUTTON_BG);
    buttonHoverBrush = CreateSolidBrush(UIColors::BUTTON_HOVER);
    
    // Create fonts
    headerFont = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, 
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, 
                             DEFAULT_PITCH | FF_DONTCARE, "Arial");
    
    normalFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, "Arial");
    
    smallFont = CreateFontA(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, "Arial");
    
    return true;
}

void UIRenderer::RenderPanel() {
    if (!memoryDC) return;
    
    // Clear the entire memory DC with black (transparent for overlay)
    RECT fullRect = { 0, 0, windowWidth, windowHeight };
    FillRect(memoryDC, &fullRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    
    // Draw semi-transparent background panel
    RECT panelRect = { panelX, panelY, panelX + panelWidth, panelY + panelHeight };
    FillRect(memoryDC, &panelRect, backgroundBrush);
    
    // Draw border
    HPEN borderPen = CreatePen(PS_SOLID, 2, UIColors::BORDER);
    HPEN oldPen = (HPEN)SelectObject(memoryDC, borderPen);
    
    Rectangle(memoryDC, panelX, panelY, panelX + panelWidth, panelY + panelHeight);
    
    SelectObject(memoryDC, oldPen);
    DeleteObject(borderPen);
}

void UIRenderer::RenderText(int x, int y, const std::string& text, COLORREF color, HFONT font) {
    if (!memoryDC) return;
    
    SetTextColor(memoryDC, color);
    SetBkMode(memoryDC, TRANSPARENT);
    
    HFONT oldFont = (HFONT)SelectObject(memoryDC, font);
    TextOutA(memoryDC, x, y, text.c_str(), (int)text.length());
    SelectObject(memoryDC, oldFont);
}

void UIRenderer::Render(const std::vector<FeatureToggle>& toggles, const PlayerStats& stats) {
    if (!overlayWindow || !menuVisible) {
        // Hide overlay when menu is not visible
        if (overlayWindow) {
            ShowWindow(overlayWindow, SW_HIDE);
        }
        return;
    }
    
    // Update overlay position to follow game window
    RECT gameRect;
    GetWindowRect(gameWindow, &gameRect);
    
    // Check if game is fullscreen by comparing window size to screen size
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int gameWidth = gameRect.right - gameRect.left;
    int gameHeight = gameRect.bottom - gameRect.top;
    bool isFullscreen = (gameWidth >= screenWidth && gameHeight >= screenHeight);
    
    // In fullscreen, use client area instead of window rect
    if (isFullscreen) {
        RECT clientRect;
        GetClientRect(gameWindow, &clientRect);
        POINT topLeft = { 0, 0 };
        ClientToScreen(gameWindow, &topLeft);
        
        gameRect.left = topLeft.x;
        gameRect.top = topLeft.y;
        gameRect.right = topLeft.x + (clientRect.right - clientRect.left);
        gameRect.bottom = topLeft.y + (clientRect.bottom - clientRect.top);
        
        gameWidth = gameRect.right - gameRect.left;
        gameHeight = gameRect.bottom - gameRect.top;
    }
    
    // Recreate bitmap if size changed
    if (gameWidth != windowWidth || gameHeight != windowHeight) {
        windowWidth = gameWidth;
        windowHeight = gameHeight;
        
        if (memoryBitmap) {
            SelectObject(memoryDC, oldBitmap);
            DeleteObject(memoryBitmap);
            memoryBitmap = CreateCompatibleBitmap(deviceContext, windowWidth, windowHeight);
            oldBitmap = (HBITMAP)SelectObject(memoryDC, memoryBitmap);
        }
    }
    
    // Move overlay to match game window position
    // Use different flags for fullscreen to ensure it's above the game
    UINT flags = SWP_NOACTIVATE | SWP_SHOWWINDOW;
    if (isFullscreen) {
        // In fullscreen, force topmost and update z-order
        SetWindowPos(overlayWindow, HWND_TOPMOST, 
                     gameRect.left, gameRect.top, 
                     windowWidth, windowHeight,
                     flags);
        // Ensure window is visible and repaint
        ShowWindow(overlayWindow, SW_SHOWNOACTIVATE);
        UpdateWindow(overlayWindow);
    } else {
        SetWindowPos(overlayWindow, HWND_TOPMOST, 
                     gameRect.left, gameRect.top, 
                     windowWidth, windowHeight,
                     flags);
    }
    
    // Store toggles for navigation
    featureToggles = toggles;
    
    // Clear memory DC with black (will be transparent via color key)
    RECT fullRect = { 0, 0, windowWidth, windowHeight };
    FillRect(memoryDC, &fullRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    
    // Draw to memory DC
    RenderPanel();
    
    // Render title
    int yOffset = 10;
    RenderText(panelX + 15, panelY + yOffset, "AC Trainer", UIColors::HEADER, headerFont);
    yOffset += 35;
    
    // Render hint - muted gray
    RenderText(panelX + 15, panelY + yOffset, "UP/DOWN: Navigate | ENTER: Toggle | INSERT: Hide", RGB(110, 115, 120), smallFont);
    yOffset += 25;
    
    // Render sections
    RenderFeatureToggles(yOffset);
    RenderPlayerStats(yOffset, stats);
    RenderUnloadButton(yOffset);
    
    // Blit to overlay window (single operation = no flicker)
    BitBlt(deviceContext, 0, 0, windowWidth, windowHeight, memoryDC, 0, 0, SRCCOPY);
    
    // Force a redraw in fullscreen mode
    if (isFullscreen) {
        RedrawWindow(overlayWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }
}

void UIRenderer::RenderFeatureToggles(int& yOffset) {
    if (!deviceContext) return;
    
    int x = panelX + 15;
    int y = panelY + yOffset;
    
    // Header
    RenderText(x, y, "=== FEATURES ===", UIColors::HEADER, normalFont);
    y += 25;
    
    // Render each toggle button
    for (size_t i = 0; i < featureToggles.size(); i++) {
        RenderToggleButton(x, y, i, featureToggles[i]);
        y += 35;
    }
    
    yOffset = y - panelY + 10;
}

void UIRenderer::RenderToggleButton(int x, int y, int index, const FeatureToggle& toggle) {
    if (!memoryDC) return;
    
    int buttonWidth = 300;
    int buttonHeight = 30;
    
    // Update bounds for hit testing
    featureToggles[index].bounds = { x, y, x + buttonWidth, y + buttonHeight };
    
    // Choose brush based on selection state (keyboard navigation)
    bool isSelected = (index == selectedIndex);
    HBRUSH brush = isSelected ? buttonHoverBrush : buttonBrush;
    
    // Draw button background
    RECT buttonRect = { x, y, x + buttonWidth, y + buttonHeight };
    FillRect(memoryDC, &buttonRect, brush);
    
    // Draw button border (highlight if selected)
    COLORREF borderColor = isSelected ? UIColors::ACTIVE : UIColors::BORDER;
    HPEN borderPen = CreatePen(PS_SOLID, isSelected ? 2 : 1, borderColor);
    HPEN oldPen = (HPEN)SelectObject(memoryDC, borderPen);
    Rectangle(memoryDC, x, y, x + buttonWidth, y + buttonHeight);
    SelectObject(memoryDC, oldPen);
    DeleteObject(borderPen);
    
    // Get active state
    bool isActive = toggle.isActive ? toggle.isActive() : false;
    
    // Draw status indicator
    HBRUSH statusBrush = isActive ? activeFeatureBrush : inactiveFeatureBrush;
    RECT statusRect = { x + 5, y + 8, x + 20, y + 22 };
    FillRect(memoryDC, &statusRect, statusBrush);
    
    // Draw feature name
    std::string displayName = "[" + std::string(isActive ? "ON" : "OFF") + "] " + toggle.name;
    RenderText(x + 25, y + 7, displayName, UIColors::TEXT, normalFont);
}

void UIRenderer::RenderPlayerStats(int& yOffset, const PlayerStats& stats) {
    if (!deviceContext) return;
    
    int x = panelX + 15;
    int y = panelY + yOffset;
    
    // Header
    RenderText(x, y, "=== STATS ===", UIColors::HEADER, normalFont);
    y += 25;
    
    // Health
    char healthStr[64];
    sprintf_s(healthStr, sizeof(healthStr), "Health: %d/100", stats.health);
    COLORREF healthColor = (stats.health > 70) ? UIColors::ACTIVE : 
                          (stats.health > 30) ? RGB(200, 160, 60) : RGB(200, 100, 100);
    RenderText(x, y, healthStr, healthColor, normalFont);
    y += 20;
    
    // Armor
    char armorStr[64];
    sprintf_s(armorStr, sizeof(armorStr), "Armor:  %d/100", stats.armor);
    COLORREF armorColor = (stats.armor > 50) ? UIColors::ACTIVE : UIColors::TEXT;
    RenderText(x, y, armorStr, armorColor, normalFont);
    y += 20;
    
    // Ammo
    char ammoStr[64];
    sprintf_s(ammoStr, sizeof(ammoStr), "Ammo:   %d", stats.ammo);
    RenderText(x, y, ammoStr, UIColors::TEXT, normalFont);
    y += 20;
    
    yOffset = y - panelY + 10;
}

void UIRenderer::RenderUnloadButton(int& yOffset) {
    if (!memoryDC) return;
    
    int x = panelX + 15;
    int y = panelY + yOffset;
    int buttonWidth = 300;
    int buttonHeight = 30;
    
    // Update unload button bounds
    unloadButtonRect = { x, y, x + buttonWidth, y + buttonHeight };
    
    // Check if unload button is selected
    bool isSelected = (selectedIndex == -1);
    
    // Draw button - muted red tones
    COLORREF bgColor = isSelected ? RGB(160, 60, 60) : RGB(120, 40, 40);
    HBRUSH brush = CreateSolidBrush(bgColor);
    RECT buttonRect = { x, y, x + buttonWidth, y + buttonHeight };
    FillRect(memoryDC, &buttonRect, brush);
    DeleteObject(brush);
    
    // Draw border (highlight if selected) - softer colors
    COLORREF borderColor = isSelected ? RGB(200, 180, 100) : RGB(180, 80, 80);
    HPEN borderPen = CreatePen(PS_SOLID, isSelected ? 2 : 1, borderColor);
    HPEN oldPen = (HPEN)SelectObject(memoryDC, borderPen);
    Rectangle(memoryDC, x, y, x + buttonWidth, y + buttonHeight);
    SelectObject(memoryDC, oldPen);
    DeleteObject(borderPen);
    
    // Draw text - softer white
    RenderText(x + 110, y + 7, "UNLOAD TRAINER", RGB(220, 220, 220), normalFont);
    
    yOffset = y - panelY + buttonHeight + 10;
}

void UIRenderer::ToggleMenu() {
    menuVisible = !menuVisible;
}

void UIRenderer::HandleKeyDown() {
    if (!menuVisible) return;
    
    int maxIndex = (int)featureToggles.size() - 1;
    selectedIndex++;
    
    // Wrap around: after last toggle goes to unload button (-1)
    if (selectedIndex > maxIndex) {
        selectedIndex = -1;
    }
}

void UIRenderer::HandleKeyUp() {
    if (!menuVisible) return;
    
    int maxIndex = (int)featureToggles.size() - 1;
    selectedIndex--;
    
    // Wrap around: before first toggle goes to unload button (-1)
    if (selectedIndex < -1) {
        selectedIndex = maxIndex;
    }
}

bool UIRenderer::HandleKeyEnter() {
    if (!menuVisible) return false;
    
    // If unload button is selected
    if (selectedIndex == -1) {
        return true; // Signal unload
    }
    
    // Toggle the selected feature
    if (selectedIndex >= 0 && selectedIndex < (int)featureToggles.size()) {
        if (featureToggles[selectedIndex].onToggle) {
            featureToggles[selectedIndex].onToggle();
        }
    }
    
    return false;
}

void UIRenderer::Shutdown() {
    // Clean up memory DC
    if (memoryDC) {
        if (memoryBitmap) {
            SelectObject(memoryDC, oldBitmap);
            DeleteObject(memoryBitmap);
            memoryBitmap = NULL;
        }
        DeleteDC(memoryDC);
        memoryDC = NULL;
    }
    
    if (deviceContext) {
        ReleaseDC(overlayWindow, deviceContext);
        deviceContext = NULL;
    }
    
    // Destroy overlay window
    if (overlayWindow) {
        DestroyWindow(overlayWindow);
        overlayWindow = NULL;
    }
    
    if (backgroundBrush) {
        DeleteObject(backgroundBrush);
        backgroundBrush = NULL;
    }
    if (activeFeatureBrush) {
        DeleteObject(activeFeatureBrush);
        activeFeatureBrush = NULL;
    }
    if (inactiveFeatureBrush) {
        DeleteObject(inactiveFeatureBrush);
        inactiveFeatureBrush = NULL;
    }
    if (buttonBrush) {
        DeleteObject(buttonBrush);
        buttonBrush = NULL;
    }
    if (buttonHoverBrush) {
        DeleteObject(buttonHoverBrush);
        buttonHoverBrush = NULL;
    }
    if (headerFont) {
        DeleteObject(headerFont);
        headerFont = NULL;
    }
    if (normalFont) {
        DeleteObject(normalFont);
        normalFont = NULL;
    }
    if (smallFont) {
        DeleteObject(smallFont);
        smallFont = NULL;
    }
}
