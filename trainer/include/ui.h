#ifndef UI_H
#define UI_H

#include <Windows.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

// UI Color Palette - Dark theme for comfortable viewing
namespace UIColors {
    static const COLORREF BACKGROUND = RGB(25, 28, 32);      // Very dark gray-blue
    static const COLORREF BORDER = RGB(60, 70, 85);          // Subtle dark blue-gray border
    static const COLORREF ACTIVE = RGB(80, 200, 120);        // Softer green (less bright)
    static const COLORREF INACTIVE = RGB(100, 105, 110);     // Muted gray
    static const COLORREF TEXT = RGB(180, 185, 190);         // Soft light gray (not white)
    static const COLORREF HEADER = RGB(120, 160, 200);       // Muted cyan-blue
    static const COLORREF BUTTON_BG = RGB(35, 38, 42);       // Slightly lighter than background
    static const COLORREF BUTTON_HOVER = RGB(45, 50, 58);    // Subtle hover highlight
}

// Feature toggle widget
struct FeatureToggle {
    std::string name;
    std::string description;
    RECT bounds;
    std::function<void()> onToggle;
    std::function<bool()> isActive;
};

// Player stats snapshot
struct PlayerStats {
    int health;
    int armor;
    int ammo;
};

class UIRenderer {
private:
    HWND gameWindow;
    HWND overlayWindow;    // Transparent overlay window
    HDC deviceContext;
    HDC memoryDC;          // Double buffer to prevent flicker
    HBITMAP memoryBitmap;
    HBITMAP oldBitmap;
    HBRUSH backgroundBrush;
    HBRUSH activeFeatureBrush;
    HBRUSH inactiveFeatureBrush;
    HBRUSH buttonBrush;
    HBRUSH buttonHoverBrush;
    HFONT headerFont;
    HFONT normalFont;
    HFONT smallFont;
    
    // UI dimensions
    int windowWidth;
    int windowHeight;
    int panelX;
    int panelY;
    int panelWidth;
    int panelHeight;
    
    // Interactive elements
    std::vector<FeatureToggle> featureToggles;
    RECT unloadButtonRect;
    int selectedIndex;     // For keyboard navigation (-1 = unload button)
    bool menuVisible;      // Menu visibility toggle
    
public:
    UIRenderer();
    ~UIRenderer();
    
    // Initialize UI system
    bool Initialize(HWND targetWindow);
    
    // Render the main UI panel with interactive toggles
    void Render(const std::vector<FeatureToggle>& toggles, const PlayerStats& stats);
    
    // Menu visibility
    void ToggleMenu();
    bool IsMenuVisible() const { return menuVisible; }
    
    // Keyboard navigation
    void HandleKeyDown();   // Arrow DOWN
    void HandleKeyUp();     // Arrow UP
    bool HandleKeyEnter();  // ENTER key - returns true if unload requested
    
    // Get selected item info
    int GetSelectedIndex() const { return selectedIndex; }
    size_t GetToggleCount() const { return featureToggles.size(); }
    
    // Render individual components
    void RenderPanel();
    void RenderFeatureToggles(int& yOffset);
    void RenderPlayerStats(int& yOffset, const PlayerStats& stats);
    void RenderUnloadButton(int& yOffset);
    void RenderText(int x, int y, const std::string& text, COLORREF color, HFONT font);
    void RenderToggleButton(int x, int y, int index, const FeatureToggle& toggle);
    
    // Cleanup
    void Shutdown();
};

#endif // UI_H
