#include "pch.h"
#include "ui.h"
#include <cstdio>

UIRenderer::UIRenderer()
    : gameWindow(NULL),
      deviceContext(NULL),
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
      hoveredToggleIndex(-1) {
}

UIRenderer::~UIRenderer() {
    Shutdown();
}

bool UIRenderer::Initialize(HWND targetWindow) {
    if (!targetWindow) {
        return false;
    }
    
    gameWindow = targetWindow;
    deviceContext = GetDC(gameWindow);
    
    if (!deviceContext) {
        return false;
    }
    
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
    
    // Get window dimensions
    RECT clientRect;
    if (GetClientRect(gameWindow, &clientRect)) {
        windowWidth = clientRect.right - clientRect.left;
        windowHeight = clientRect.bottom - clientRect.top;
    }
    
    return true;
}
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, "Arial");
    
    smallFont = CreateFontA(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, "Arial");
    
    // Get window dimensions
    RECT rect;
    if (GetClientRect(gameWindow, &rect)) {
        windowWidth = rect.right - rect.left;
        windowHeight = rect.bottom - rect.top;
    }
    
    return true;
}

void UIRenderer::RenderPanel() {
    if (!deviceContext) return;
    
    // Draw semi-transparent background panel
    RECT panelRect = { panelX, panelY, panelX + panelWidth, panelY + panelHeight };
    FillRect(deviceContext, &panelRect, backgroundBrush);
    
    // Draw border
    HPEN borderPen = CreatePen(PS_SOLID, 2, UIColors::BORDER);
    HPEN oldPen = (HPEN)SelectObject(deviceContext, borderPen);
    
    Rectangle(deviceContext, panelX, panelY, panelX + panelWidth, panelY + panelHeight);
    
    SelectObject(deviceContext, oldPen);
    DeleteObject(borderPen);
}

void UIRenderer::RenderText(int x, int y, const std::string& text, COLORREF color, HFONT font) {
    if (!deviceContext) return;
    
    SetTextColor(deviceContext, color);
    SetBkMode(deviceContext, TRANSPARENT);
    
    HFONT oldFont = (HFONT)SelectObject(deviceContext, font);
    TextOutA(deviceContext, x, y, text.c_str(), (int)text.length());
    SelectObject(deviceContext, oldFont);
}

void UIRenderer::RenderFeatureStatus(int yOffset, bool godMode, bool infiniteAmmo, bool noRecoil) {
    if (!deviceContext) return;
    
    int x = panelX + 15;
    int y = panelY + yOffset;
    
    // Header
    RenderText(x, y, "=== FEATURES ===", UIColors::HEADER, headerFont);
    y += 25;
    
    // God Mode
    std::string godModeStatus = godMode ? "[ON]  God Mode" : "[OFF] God Mode";
    RenderText(x, y, godModeStatus, godMode ? UIColors::ACTIVE : UIColors::INACTIVE, normalFont);
    y += 20;
    
    // Infinite Ammo
    std::string ammoStatus = infiniteAmmo ? "[ON]  Infinite Ammo" : "[OFF] Infinite Ammo";
    RenderText(x, y, ammoStatus, infiniteAmmo ? UIColors::ACTIVE : UIColors::INACTIVE, normalFont);
    y += 20;
    
    // No Recoil
    std::string recoilStatus = noRecoil ? "[ON]  No Recoil" : "[OFF] No Recoil";
    RenderText(x, y, recoilStatus, noRecoil ? UIColors::ACTIVE : UIColors::INACTIVE, normalFont);
}

void UIRenderer::RenderPlayerStats(int yOffset, int health, int armor, int ammo) {
    if (!deviceContext) return;
    
    int x = panelX + 15;
    int y = panelY + yOffset;
    
    // Header
    RenderText(x, y, "=== STATS ===", UIColors::HEADER, headerFont);
    y += 25;
    
    // Health
    char healthStr[64];
    sprintf_s(healthStr, sizeof(healthStr), "Health: %d/100", health);
    COLORREF healthColor = (health > 70) ? UIColors::ACTIVE : (health > 30) ? RGB(255, 200, 0) : RGB(255, 100, 100);
    RenderText(x, y, healthStr, healthColor, normalFont);
    y += 20;
    
    // Armor
    char armorStr[64];
    sprintf_s(armorStr, sizeof(armorStr), "Armor:  %d/100", armor);
    COLORREF armorColor = (armor > 50) ? UIColors::ACTIVE : UIColors::TEXT;
    RenderText(x, y, armorStr, armorColor, normalFont);
    y += 20;
    
    // Ammo
    char ammoStr[64];
    sprintf_s(ammoStr, sizeof(ammoStr), "Ammo:   %d", ammo);
    RenderText(x, y, ammoStr, UIColors::TEXT, normalFont);
}

void UIRenderer::RenderHotkeyHelp(int yOffset) {
    if (!deviceContext) return;
    
    int x = panelX + 15;
    int y = panelY + yOffset;
    
    // Header
    RenderText(x, y, "=== HOTKEYS ===", UIColors::HEADER, smallFont);
    y += 20;
    
    RenderText(x, y, "F1: God Mode", UIColors::INACTIVE, smallFont);
    y += 16;
    RenderText(x, y, "F2: Infinite Ammo", UIColors::INACTIVE, smallFont);
    y += 16;
    RenderText(x, y, "F3: No Recoil", UIColors::INACTIVE, smallFont);
    y += 16;
    RenderText(x, y, "F4: Add Health", UIColors::INACTIVE, smallFont);
    y += 16;
    RenderText(x, y, "END: Unload", RGB(255, 100, 100), smallFont);
}

void UIRenderer::Render(bool godMode, bool infiniteAmmo, bool noRecoil,
                        int health, int armor, int ammo) {
    if (!gameWindow) return;
    
    // Get fresh device context each frame (it may have been invalidated)
    HDC currentDC = GetDC(gameWindow);
    if (!currentDC) return;
    
    // Temporarily use the fresh DC
    HDC oldDC = deviceContext;
    deviceContext = currentDC;
    
    // Draw main panel
    RenderPanel();
    
    // Render sections
    RenderFeatureStatus(10, godMode, infiniteAmmo, noRecoil);
    RenderPlayerStats(100, health, armor, ammo);
    RenderHotkeyHelp(160);
    
    // Release the fresh DC
    ReleaseDC(gameWindow, currentDC);
    deviceContext = oldDC;
}

void UIRenderer::Shutdown() {
    if (deviceContext) {
        ReleaseDC(gameWindow, deviceContext);
        deviceContext = NULL;
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
