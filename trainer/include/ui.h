#ifndef UI_H
#define UI_H

#include <Windows.h>
#include <functional>
#include <string>
#include <vector>

class Trainer;
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

// UI Tab enumeration
enum class UITab {
    MISC = 0,
    AIMBOT = 1,
    ESP = 2
};

class UIRenderer {
private:
    HWND gameWindow;
    bool imguiInitialized;
    bool menuVisible;
    bool insertHeld;
    bool upHeld;
    bool downHeld;
    bool enterHeld;
    bool unloadRequestPending;
    RECT savedClipRect;
    bool hasSavedClipRect;
    std::function<void(bool)> onMenuVisibilityChanged;

    int selectedIndex;
    int panelWidth;
    int panelPadding;
    int sectionSpacing;

    // Drag-and-drop state
    bool isDragging;
    float panelPosX;
    float panelPosY;
    float dragOffsetX;
    float dragOffsetY;

    // Tab navigation
    UITab currentTab;

    ImFont* headerFont;
    ImFont* textFont;
    ImFont* smallFont;

    std::vector<FeatureToggle> featureToggles;

    bool InitializeImGui();
    void UpdateMenuState();
    void UpdateNavigation(Trainer& trainer);
    void DrawMenu(const PlayerStats& stats, Trainer& trainer);
    float DrawTabBar(struct ImDrawList* drawList, const struct ImVec2& start, float width);
    float DrawMiscTab(struct ImDrawList* drawList, const struct ImVec2& start, Trainer& trainer, const PlayerStats& stats);
    float DrawAimbotTab(struct ImDrawList* drawList, const struct ImVec2& start, Trainer& trainer);
    float DrawESPTab(struct ImDrawList* drawList, const struct ImVec2& start, Trainer& trainer);
    float DrawFeatureToggles(struct ImDrawList* drawList, const struct ImVec2& start);
    float DrawPlayerStats(struct ImDrawList* drawList, const struct ImVec2& start, const PlayerStats& stats);
    float DrawAimbotSettings(struct ImDrawList* drawList, const struct ImVec2& start, Trainer& trainer);
    float DrawUnloadButton(struct ImDrawList* drawList, const struct ImVec2& start, bool& requestUnload);

public:
    UIRenderer();
    ~UIRenderer();

    // Initialize UI system
    bool Initialize(HWND targetWindow);

    // Render the main UI panel with interactive toggles
    void Render(Trainer& trainer);
    
    // ESP rendering
    void RenderESP(Trainer& trainer);

    bool IsImGuiInitialized() const { return imguiInitialized; }

    // Menu visibility
    void ToggleMenu();
    bool IsMenuVisible() const { return menuVisible; }
    void SetMenuVisible(bool visible);
    void SetMenuVisibilityCallback(std::function<void(bool)> callback) { onMenuVisibilityChanged = callback; }

    // Keyboard navigation
    void HandleKeyDown();   // Arrow DOWN
    void HandleKeyUp();     // Arrow UP
    bool HandleKeyEnter();  // ENTER key - returns true if unload requested

    // Message pump input handling
    bool ProcessInput(MSG& msg, bool& requestUnload);

    // Get selected item info
    int GetSelectedIndex() const { return selectedIndex; }
    size_t GetToggleCount() const { return featureToggles.size(); }
    
    // Cleanup
    void Shutdown();
};

#endif // UI_H
