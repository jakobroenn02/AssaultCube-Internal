#ifndef UI_H
#define UI_H

#include <Windows.h>
#include <functional>
#include <string>
#include <vector>

class Trainer;
struct IDirect3DDevice9;
struct ImFont;
struct ImDrawList;
struct ImVec2;

// Feature toggle widget
struct FeatureToggle {
    std::string name;
    std::string description;
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
    IDirect3DDevice9* device;

    bool imguiInitialized;
    bool menuVisible;
    bool insertHeld;
    bool upHeld;
    bool downHeld;
    bool enterHeld;

    int selectedIndex;
    int panelWidth;
    int panelPadding;
    int sectionSpacing;

    ImFont* headerFont;
    ImFont* textFont;
    ImFont* smallFont;

    std::vector<FeatureToggle> featureToggles;

    void InitializeImGui(IDirect3DDevice9* d3dDevice);
    void UpdateMenuState();
    void UpdateNavigation(Trainer& trainer);
    void DrawMenu(const PlayerStats& stats);
    float DrawFeatureToggles(struct ImDrawList* drawList, const struct ImVec2& start);
    float DrawPlayerStats(struct ImDrawList* drawList, const struct ImVec2& start, const PlayerStats& stats);
    float DrawUnloadButton(struct ImDrawList* drawList, const struct ImVec2& start);

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
    void SetMenuVisible(bool visible);

    // Keyboard navigation
    void HandleKeyDown();   // Arrow DOWN
    void HandleKeyUp();     // Arrow UP
    bool HandleKeyEnter();  // ENTER key - returns true if unload requested

    // Message pump input handling
    bool ProcessInput(MSG& msg, bool& requestUnload);

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
    
    // this should may or may not be here
    void Render(IDirect3DDevice9* device, Trainer& trainer);

    // Cleanup
    void Shutdown();
};

#endif // UI_H
