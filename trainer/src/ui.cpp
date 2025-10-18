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

void UIRenderer::Render(const std::vector<FeatureToggle>& toggles, const PlayerStats& stats) {
    if (!gameWindow) return;
    
    // Get fresh device context each frame
    HDC currentDC = GetDC(gameWindow);
    if (!currentDC) return;
    
    HDC oldDC = deviceContext;
    deviceContext = currentDC;
    
    // Store toggles for hit testing
    featureToggles = toggles;
    
    // Draw main panel
    RenderPanel();
    
    // Render title
    int yOffset = 10;
    RenderText(panelX + 15, panelY + yOffset, "AC Trainer", UIColors::HEADER, headerFont);
    yOffset += 35;
    
    // Render sections
    RenderFeatureToggles(yOffset);
    RenderPlayerStats(yOffset, stats);
    RenderUnloadButton(yOffset);
    
    // Release the fresh DC
    ReleaseDC(gameWindow, currentDC);
    deviceContext = oldDC;
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
    if (!deviceContext) return;
    
    int buttonWidth = 300;
    int buttonHeight = 30;
    
    // Update bounds for hit testing
    featureToggles[index].bounds = { x, y, x + buttonWidth, y + buttonHeight };
    
    // Choose brush based on hover state
    HBRUSH brush = (index == hoveredToggleIndex) ? buttonHoverBrush : buttonBrush;
    
    // Draw button background
    RECT buttonRect = { x, y, x + buttonWidth, y + buttonHeight };
    FillRect(deviceContext, &buttonRect, brush);
    
    // Draw button border
    HPEN borderPen = CreatePen(PS_SOLID, 1, UIColors::BORDER);
    HPEN oldPen = (HPEN)SelectObject(deviceContext, borderPen);
    Rectangle(deviceContext, x, y, x + buttonWidth, y + buttonHeight);
    SelectObject(deviceContext, oldPen);
    DeleteObject(borderPen);
    
    // Get active state
    bool isActive = toggle.isActive ? toggle.isActive() : false;
    
    // Draw status indicator
    HBRUSH statusBrush = isActive ? activeFeatureBrush : inactiveFeatureBrush;
    RECT statusRect = { x + 5, y + 8, x + 20, y + 22 };
    FillRect(deviceContext, &statusRect, statusBrush);
    
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
                          (stats.health > 30) ? RGB(255, 200, 0) : RGB(255, 100, 100);
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
    if (!deviceContext) return;
    
    int x = panelX + 15;
    int y = panelY + yOffset;
    int buttonWidth = 300;
    int buttonHeight = 30;
    
    // Update unload button bounds
    unloadButtonRect = { x, y, x + buttonWidth, y + buttonHeight };
    
    // Draw button
    HBRUSH brush = CreateSolidBrush(RGB(180, 50, 50));
    RECT buttonRect = { x, y, x + buttonWidth, y + buttonHeight };
    FillRect(deviceContext, &buttonRect, brush);
    DeleteObject(brush);
    
    // Draw border
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(255, 100, 100));
    HPEN oldPen = (HPEN)SelectObject(deviceContext, borderPen);
    Rectangle(deviceContext, x, y, x + buttonWidth, y + buttonHeight);
    SelectObject(deviceContext, oldPen);
    DeleteObject(borderPen);
    
    // Draw text
    RenderText(x + 110, y + 7, "UNLOAD TRAINER", RGB(255, 255, 255), normalFont);
    
    yOffset = y - panelY + buttonHeight + 10;
}

bool UIRenderer::HandleMouseClick(int x, int y) {
    // Check unload button
    if (x >= unloadButtonRect.left && x <= unloadButtonRect.right &&
        y >= unloadButtonRect.top && y <= unloadButtonRect.bottom) {
        return true; // Signal unload
    }
    
    // Check feature toggles
    for (const auto& toggle : featureToggles) {
        if (x >= toggle.bounds.left && x <= toggle.bounds.right &&
            y >= toggle.bounds.top && y <= toggle.bounds.bottom) {
            if (toggle.onToggle) {
                toggle.onToggle();
            }
            break;
        }
    }
    
    return false;
}

void UIRenderer::HandleMouseMove(int x, int y) {
    hoveredToggleIndex = -1;
    
    for (size_t i = 0; i < featureToggles.size(); i++) {
        const auto& toggle = featureToggles[i];
        if (x >= toggle.bounds.left && x <= toggle.bounds.right &&
            y >= toggle.bounds.top && y <= toggle.bounds.bottom) {
            hoveredToggleIndex = (int)i;
            break;
        }
    }
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
