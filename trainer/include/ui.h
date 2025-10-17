#ifndef UI_H
#define UI_H

#include <Windows.h>
#include <string>
#include <vector>
#include <cstdint>

// UI Color Palette
namespace UIColors {
    static const COLORREF BACKGROUND = RGB(20, 20, 30);     // Dark blue-gray
    static const COLORREF BORDER = RGB(100, 150, 200);      // Bright blue
    static const COLORREF ACTIVE = RGB(0, 255, 100);        // Bright green
    static const COLORREF INACTIVE = RGB(150, 150, 150);    // Gray
    static const COLORREF TEXT = RGB(200, 200, 200);        // Light gray
    static const COLORREF HEADER = RGB(100, 200, 255);      // Cyan
}

class UIRenderer {
private:
    HWND gameWindow;
    HDC deviceContext;
    HBRUSH backgroundBrush;
    HBRUSH activeFeatureBrush;
    HBRUSH inactiveFeatureBrush;
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
    
public:
    UIRenderer();
    ~UIRenderer();
    
    // Initialize UI system
    bool Initialize(HWND targetWindow);
    
    // Render the main UI panel
    void Render(bool godMode, bool infiniteAmmo, bool noRecoil, 
                int health, int armor, int ammo);
    
    // Render individual components
    void RenderPanel();
    void RenderFeatureStatus(int yOffset, bool godMode, bool infiniteAmmo, bool noRecoil);
    void RenderPlayerStats(int yOffset, int health, int armor, int ammo);
    void RenderHotkeyHelp(int yOffset);
    void RenderText(int x, int y, const std::string& text, COLORREF color, HFONT font);
    
    // Cleanup
    void Shutdown();
};

#endif // UI_H
