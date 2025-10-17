#ifndef UI_H
#define UI_H

#include <Windows.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

// UI Color Palette
namespace UIColors {
    static const COLORREF BACKGROUND = RGB(20, 20, 30);     // Dark blue-gray
    static const COLORREF BORDER = RGB(100, 150, 200);      // Bright blue
    static const COLORREF ACTIVE = RGB(0, 255, 100);        // Bright green
    static const COLORREF INACTIVE = RGB(150, 150, 150);    // Gray
    static const COLORREF TEXT = RGB(200, 200, 200);        // Light gray
    static const COLORREF HEADER = RGB(100, 200, 255);      // Cyan
    static const COLORREF BUTTON_BG = RGB(40, 40, 50);      // Darker background for buttons
    static const COLORREF BUTTON_HOVER = RGB(60, 60, 80);   // Button hover state
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
    HDC deviceContext;
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
    int hoveredToggleIndex;
    
public:
    UIRenderer();
    ~UIRenderer();
    
    // Initialize UI system
    bool Initialize(HWND targetWindow);
    
    // Render the main UI panel with interactive toggles
    void Render(const std::vector<FeatureToggle>& toggles, const PlayerStats& stats);
    
    // Input handling
    bool HandleMouseClick(int x, int y);
    bool HandleMouseMove(int x, int y);
    
    // Get bounds for hit testing
    const std::vector<FeatureToggle>& GetToggles() const { return featureToggles; }
    const RECT& GetUnloadButtonRect() const { return unloadButtonRect; }
    
    // Render individual components
    void RenderPanel();
    void RenderFeatureToggles(int& yOffset);
    void RenderPlayerStats(int& yOffset, const PlayerStats& stats);
    void RenderUnloadButton(int& yOffset);
    void RenderText(int x, int y, const std::string& text, COLORREF color, HFONT font);
    void RenderToggleButton(int x, int y, int width, int height, const std::string& text, bool isActive, bool isHovered);
    
    // Cleanup
    void Shutdown();
};

#endif // UI_H
