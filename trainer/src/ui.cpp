#include "pch.h"
#include "ui.h"
#include "trainer.h"
#include "memory.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include <cstdio>
#include <thread>
#include <cmath>
#include <windowsx.h>

// Simple vector structs for matrix math
struct Vec3 { float x, y, z; };

namespace {

struct ViewMatrix {
    float data[16];
};

// AssaultCube exposes a row-major view-projection matrix in memory. Treating the
// data as DirectX-style rows matches the approach used by external overlays such as
// GuidedHacking's AssaultCube ESP examples and keeps our projections aligned with
// the in-game renderer.
bool WorldToScreenMatrix(const Vec3& pos, float screen[2], const ViewMatrix& matrix, int width, int height) {
    const float* m = matrix.data;

    float clipX = pos.x * m[0] + pos.y * m[4] + pos.z * m[8] + m[12];
    float clipY = pos.x * m[1] + pos.y * m[5] + pos.z * m[9] + m[13];
    float clipW = pos.x * m[3] + pos.y * m[7] + pos.z * m[11] + m[15];

    if (clipW < 0.1f) {
        return false;
    }

    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;

    screen[0] = (width * 0.5f) + (ndcX * width * 0.5f);
    screen[1] = (height * 0.5f) - (ndcY * height * 0.5f);
    return true;
}

} // namespace

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

namespace {
constexpr ImU32 ColorU32(int r, int g, int b, int a = 255) {
    return IM_COL32(r, g, b, a);
}

constexpr float kButtonHeight = 32.0f;
constexpr float kButtonSpacing = 8.0f;
constexpr float kIndicatorSize = 14.0f;
constexpr float kCornerRadius = 8.0f;
} // namespace

UIRenderer::UIRenderer()
    : gameWindow(nullptr),
      imguiInitialized(false),
      menuVisible(false),
      insertHeld(false),
      upHeld(false),
      downHeld(false),
      enterHeld(false),
      unloadRequestPending(false),
      hasSavedClipRect(false),
      selectedIndex(0),
      panelWidth(360),
      panelPadding(18),
      sectionSpacing(22),
      headerFont(nullptr),
      textFont(nullptr),
      smallFont(nullptr) {
    savedClipRect = {0, 0, 0, 0};
}

UIRenderer::~UIRenderer() {
    Shutdown();
}

bool UIRenderer::Initialize(HWND targetWindow) {
    if (!targetWindow) {
        return false;
    }

    gameWindow = targetWindow;
    menuVisible = false;
    selectedIndex = 0;
    featureToggles.clear();
    return true;
}

bool UIRenderer::InitializeImGui() {
    if (imguiInitialized) {
        return true;
    }

    if (!gameWindow) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.11f, 0.13f, 1.0f);

    ImFontConfig textConfig;
    textConfig.SizePixels = 16.0f;
    textFont = io.Fonts->AddFontDefault(&textConfig);

    ImFontConfig headerConfig;
    headerConfig.SizePixels = 22.0f;
    headerFont = io.Fonts->AddFontDefault(&headerConfig);

    ImFontConfig smallConfig;
    smallConfig.SizePixels = 13.0f;
    smallFont = io.Fonts->AddFontDefault(&smallConfig);

    if (!textFont) {
        textFont = io.Fonts->AddFontDefault();
    }
    if (!headerFont) {
        headerFont = textFont;
    }
    if (!smallFont) {
        smallFont = textFont;
    }

    if (!ImGui_ImplWin32_InitForOpenGL(gameWindow)) {
        std::cout << "Failed to initialize ImGui Win32 backend for OpenGL." << std::endl;
        return false;
    }

    ImGui_ImplWin32_EnableAlphaCompositing(gameWindow);

    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        std::cout << "Failed to initialize ImGui OpenGL3 backend." << std::endl;
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    imguiInitialized = true;
    return true;
}

void UIRenderer::UpdateMenuState() {
    // Note: INSERT key is handled by the message hook in dllmain.cpp
    // which calls SetMenuVisible() directly. We don't check it here
    // to avoid conflicts between GetAsyncKeyState and the message hook.
    
    if (!menuVisible) {
        selectedIndex = 0;
        upHeld = downHeld = enterHeld = false;
    }
}

void UIRenderer::UpdateNavigation(Trainer& trainer) {
    if (!menuVisible) {
        return;
    }

    int maxIndex = static_cast<int>(featureToggles.size()) - 1;
    if (maxIndex < -1) {
        maxIndex = -1;
    }

    if (selectedIndex > maxIndex) {
        selectedIndex = maxIndex;
    }
    if (selectedIndex < -1) {
        selectedIndex = -1;
    }

    bool downPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
    if (downPressed && !downHeld) {
        selectedIndex++;
        if (selectedIndex > maxIndex) {
            selectedIndex = -1;
        }
    }
    downHeld = downPressed;

    bool upPressed = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
    if (upPressed && !upHeld) {
        selectedIndex--;
        if (selectedIndex < -1) {
            selectedIndex = maxIndex;
        }
    }
    upHeld = upPressed;

    bool enterPressed = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    if (enterPressed && !enterHeld) {
        if (selectedIndex == -1) {
            trainer.RequestUnload();
        } else if (selectedIndex >= 0 && selectedIndex < static_cast<int>(featureToggles.size())) {
            if (featureToggles[selectedIndex].onToggle) {
                featureToggles[selectedIndex].onToggle();
            }
        }
    }
    enterHeld = enterPressed;
}

void UIRenderer::HandleKeyDown() {
    if (!menuVisible) return;
    
    int maxIndex = static_cast<int>(featureToggles.size()) - 1;
    selectedIndex++;
    if (selectedIndex > maxIndex) {
        selectedIndex = -1;  // Wrap to unload button
    }
}

void UIRenderer::HandleKeyUp() {
    if (!menuVisible) return;
    
    int maxIndex = static_cast<int>(featureToggles.size()) - 1;
    selectedIndex--;
    if (selectedIndex < -1) {
        selectedIndex = maxIndex;  // Wrap to last feature
    }
}

float UIRenderer::DrawFeatureToggles(ImDrawList* drawList, const ImVec2& start) {
    float y = start.y;
    const float buttonWidth = static_cast<float>(panelWidth - panelPadding * 2);
    ImGuiIO& io = ImGui::GetIO();

    for (size_t i = 0; i < featureToggles.size(); ++i) {
        const bool isActive = featureToggles[i].isActive ? featureToggles[i].isActive() : false;
        
        ImVec2 minVec(start.x, y);
        ImVec2 maxVec(start.x + buttonWidth, y + kButtonHeight);
        
        // Check if mouse is hovering over this button
        bool isHovered = io.MousePos.x >= minVec.x && io.MousePos.x <= maxVec.x &&
                        io.MousePos.y >= minVec.y && io.MousePos.y <= maxVec.y;
        
        // Check for click
        if (isHovered && ImGui::IsMouseClicked(0)) {
            if (featureToggles[i].onToggle) {
                featureToggles[i].onToggle();
            }
        }
        
        const bool isSelected = isHovered;

        ImU32 bgColor = isSelected ? ColorU32(45, 50, 58) : ColorU32(35, 38, 42);
        ImU32 borderColor = isSelected ? ColorU32(80, 200, 120) : ColorU32(60, 70, 85);
        ImU32 indicatorColor = isActive ? ColorU32(80, 200, 120) : ColorU32(100, 105, 110);

        drawList->AddRectFilled(minVec, maxVec, bgColor, kCornerRadius);
        drawList->AddRect(minVec, maxVec, borderColor, kCornerRadius, 0, isSelected ? 2.0f : 1.0f);

        ImVec2 indicatorMin(minVec.x + 10.0f, minVec.y + (kButtonHeight - kIndicatorSize) * 0.5f);
        ImVec2 indicatorMax(indicatorMin.x + kIndicatorSize, indicatorMin.y + kIndicatorSize);
        drawList->AddRectFilled(indicatorMin, indicatorMax, indicatorColor, 4.0f);

        std::string label = "[";
        label += isActive ? "ON" : "OFF";
        label += "] ";
        label += featureToggles[i].name;

        ImVec2 textPos(indicatorMax.x + 10.0f, minVec.y + 7.0f);
        drawList->AddText(textFont ? textFont : ImGui::GetFont(),
                          textFont ? textFont->FontSize : ImGui::GetFontSize(),
                          textPos,
                          ColorU32(180, 185, 190),
                          label.c_str());

        y += kButtonHeight + kButtonSpacing;
    }

    return y + 6.0f;
}

float UIRenderer::DrawPlayerStats(ImDrawList* drawList, const ImVec2& start, const PlayerStats& stats) {
    float y = start.y;

    drawList->AddText(textFont ? textFont : ImGui::GetFont(),
                      textFont ? textFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x, y),
                      ColorU32(120, 160, 200),
                      "=== STATS ===");
    y += 26.0f;

    auto addStat = [&](const char* label, int value, ImU32 color) {
        char buffer[64];
        sprintf_s(buffer, "%s %d", label, value);
        drawList->AddText(textFont ? textFont : ImGui::GetFont(),
                          textFont ? textFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x, y),
                          color,
                          buffer);
        y += 22.0f;
    };

    ImU32 healthColor = stats.health > 70 ? ColorU32(80, 200, 120)
                                          : (stats.health > 30 ? ColorU32(200, 160, 60) : ColorU32(200, 100, 100));
    addStat("Health:", stats.health, healthColor);
    ImU32 armorColor = stats.armor > 50 ? ColorU32(80, 200, 120) : ColorU32(180, 185, 190);
    addStat("Armor:", stats.armor, armorColor);
    addStat("Ammo:", stats.ammo, ColorU32(180, 185, 190));

    return y + sectionSpacing;
}

float UIRenderer::DrawUnloadButton(ImDrawList* drawList, const ImVec2& start, bool& requestUnload) {
    float y = start.y;
    const float buttonWidth = static_cast<float>(panelWidth - panelPadding * 2);
    ImVec2 minVec(start.x, y);
    ImVec2 maxVec(start.x + buttonWidth, y + kButtonHeight);

    ImGuiIO& io = ImGui::GetIO();
    bool isHovered = io.MousePos.x >= minVec.x && io.MousePos.x <= maxVec.x &&
                    io.MousePos.y >= minVec.y && io.MousePos.y <= maxVec.y;
    
    // Check for click
    if (isHovered && ImGui::IsMouseClicked(0)) {
        requestUnload = true;
    }

    const bool isSelected = isHovered;
    ImU32 bgColor = isSelected ? ColorU32(160, 60, 60) : ColorU32(120, 40, 40);
    ImU32 borderColor = isSelected ? ColorU32(200, 180, 100) : ColorU32(180, 80, 80);

    drawList->AddRectFilled(minVec, maxVec, bgColor, kCornerRadius);
    drawList->AddRect(minVec, maxVec, borderColor, kCornerRadius, 0, isSelected ? 2.0f : 1.0f);

    const char* text = "UNLOAD TRAINER";
    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImVec2 textPos(minVec.x + (buttonWidth - textSize.x) * 0.5f, minVec.y + (kButtonHeight - textSize.y) * 0.5f);

    drawList->AddText(textFont ? textFont : ImGui::GetFont(),
                      textFont ? textFont->FontSize : ImGui::GetFontSize(),
                      textPos,
                      ColorU32(220, 220, 220),
                      text);

    return y + kButtonHeight + 6.0f;
}

void UIRenderer::ToggleMenu() {
    SetMenuVisible(!menuVisible);
}

void UIRenderer::SetMenuVisible(bool visible) {
    if (menuVisible == visible) {
        return;
    }

    menuVisible = visible;
    
    // Notify callback about visibility change
    if (onMenuVisibilityChanged) {
        onMenuVisibilityChanged(visible);
    }

    if (!menuVisible) {
        selectedIndex = 0;
        
        // Restore the game's original cursor clipping when menu closes
        if (hasSavedClipRect) {
            std::cout << "[UI] Restoring cursor clip to game's lock" << std::endl;
            ClipCursor(&savedClipRect);
            hasSavedClipRect = false;
        } else {
            std::cout << "[UI] No saved clip rect to restore" << std::endl;
        }
        
        // Hide the ImGui cursor
        ShowCursor(FALSE);
    } else {
        // Save the current cursor clipping (game's lock) before unlocking
        if (GetClipCursor(&savedClipRect)) {
            hasSavedClipRect = true;
            std::cout << "[UI] Saved cursor clip rect: (" << savedClipRect.left << "," << savedClipRect.top 
                     << ") to (" << savedClipRect.right << "," << savedClipRect.bottom << ")" << std::endl;
        } else {
            std::cout << "[UI] Failed to get current cursor clip rect" << std::endl;
        }
        
        // Unlock the cursor when menu opens
        std::cout << "[UI] Unlocking cursor with ClipCursor(NULL)" << std::endl;
        ClipCursor(NULL);  // Remove cursor clipping
        ShowCursor(TRUE);  // Show the cursor
        
        // Verify it worked
        RECT currentClip;
        if (GetClipCursor(&currentClip)) {
            std::cout << "[UI] Current clip after unlock: (" << currentClip.left << "," << currentClip.top 
                     << ") to (" << currentClip.right << "," << currentClip.bottom << ")" << std::endl;
        }
    }
}

void UIRenderer::DrawMenu(const PlayerStats& stats) {
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 panelPos(24.0f, 24.0f);

    const float headerHeight = 36.0f;
    const float hintHeight = 28.0f;
    const float togglesHeight = static_cast<float>(featureToggles.size()) * (kButtonHeight + kButtonSpacing) + 6.0f;
    const float statsHeight = 26.0f + (22.0f * 3.0f) + static_cast<float>(sectionSpacing);
    const float unloadHeight = kButtonHeight + 6.0f;

    const float panelHeight = static_cast<float>(panelPadding) * 2.0f +
                              headerHeight + hintHeight + togglesHeight + statsHeight + unloadHeight;

    ImVec2 panelMin = panelPos;
    ImVec2 panelMax(panelPos.x + static_cast<float>(panelWidth), panelPos.y + panelHeight);

    drawList->AddRectFilled(panelMin, panelMax, ColorU32(25, 28, 32, 235), 12.0f);
    drawList->AddRect(panelMin, panelMax, ColorU32(60, 70, 85, 255), 12.0f, 0, 2.0f);

    float y = panelPos.y + static_cast<float>(panelPadding);

    // Draw header
    drawList->AddText(headerFont ? headerFont : ImGui::GetFont(),
                      headerFont ? headerFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(panelPos.x + panelPadding, y),
                      ColorU32(120, 160, 200),
                      "AC Trainer");
    y += headerHeight;

    const char* hint = "Click to toggle features | INSERT: Hide";
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(panelPos.x + panelPadding, y),
                      ColorU32(150, 150, 155),
                      hint);
    y += hintHeight;

    ImVec2 sectionStart(panelPos.x + panelPadding, y);
    y = DrawFeatureToggles(drawList, sectionStart);

    sectionStart = ImVec2(panelPos.x + panelPadding, y);
    y = DrawPlayerStats(drawList, sectionStart, stats);

    sectionStart = ImVec2(panelPos.x + panelPadding, y);
    bool unloadRequested = false;
    y = DrawUnloadButton(drawList, sectionStart, unloadRequested);
    
    if (unloadRequested) {
        unloadRequestPending = true;
    }
}

bool UIRenderer::HandleKeyEnter() {
    if (!menuVisible) return false;

    // If unload button is selected
    if (selectedIndex == -1) {
        return true; // Signal unload
    }

    // Toggle selected feature
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(featureToggles.size())) {
        if (featureToggles[selectedIndex].onToggle) {
            featureToggles[selectedIndex].onToggle();
        }
    }

    return false;
}

void UIRenderer::Render(Trainer& trainer) {
    if (!gameWindow) {
        return;
    }

    HGLRC currentContext = wglGetCurrentContext();
    if (!currentContext) {
        return;
    }

    if (!imguiInitialized) {
        if (!InitializeImGui()) {
            return;
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Control ImGui cursor visibility based on menu state
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = menuVisible;  // Only draw cursor when menu is visible
    
    // Keep cursor unlocked while menu is visible (game may try to re-lock it every frame)
    static bool firstFrame = true;
    if (menuVisible) {
        // Completely unlock cursor - no clipping at all
        ClipCursor(NULL);
        
        // On first frame of menu opening, move cursor away from center to UI
        if (firstFrame) {
            POINT cursorPos;
            GetCursorPos(&cursorPos);
            ScreenToClient(gameWindow, &cursorPos);
            
            // If cursor is near center, move it to the menu position
            RECT clientRect;
            GetClientRect(gameWindow, &clientRect);
            int centerX = clientRect.right / 2;
            int centerY = clientRect.bottom / 2;
            
            // Check if cursor is within 50 pixels of center
            if (abs(cursorPos.x - centerX) < 50 && abs(cursorPos.y - centerY) < 50) {
                // Move cursor to menu area (top-left where our menu is)
                POINT newPos = {100, 100};
                ClientToScreen(gameWindow, &newPos);
                SetCursorPos(newPos.x, newPos.y);
            }
            
            firstFrame = false;
        }
    } else {
        firstFrame = true;
    }

    UpdateMenuState();

    // ALWAYS draw a test rectangle to verify rendering works
    ImDrawList* testDrawList = ImGui::GetForegroundDrawList();
    testDrawList->AddRectFilled(ImVec2(10, 10), ImVec2(110, 60), IM_COL32(255, 0, 0, 255));
    testDrawList->AddText(ImVec2(15, 15), IM_COL32(255, 255, 255, 255), "HOOK ACTIVE");
    
    // Render ESP if enabled (before menu so it's behind)
    if (trainer.IsESPEnabled()) {
        RenderESP(trainer);
    }

    if (menuVisible) {
        featureToggles = trainer.BuildFeatureToggles();
        PlayerStats stats = trainer.GetPlayerStats();
        UpdateNavigation(trainer);
        DrawMenu(stats);
        
        // Check if unload was requested via mouse click
        if (unloadRequestPending) {
            trainer.RequestUnload();
            unloadRequestPending = false;
        }
        // TODO: Mouse lock memory writes disabled temporarily (were causing crashes)
        // Need to verify correct addresses and add error handling
    } else {
        featureToggles.clear();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool UIRenderer::ProcessInput(MSG& msg, bool& requestUnload) {
    requestUnload = false;

    if (!menuVisible) {
        // When menu is hidden, don't consume any input
        return false;
    }

    bool handled = false;

    switch (msg.message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        bool initialPress = ((msg.lParam & (1 << 30)) == 0);
        switch (msg.wParam) {
        case VK_ESCAPE:
            if (initialPress) {
                SetMenuVisible(false);
            }
            handled = true;
            break;
        default:
            break;
        }
        break;
    }
    
    // Don't block mouse input - let the game handle it
    // ImGui will handle clicks on our UI elements
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
        // Don't consume mouse messages - let them pass through to the game
        handled = false;
        break;

    default:
        break;
    }

    return handled;
}

void UIRenderer::Shutdown() {
    if (imguiInitialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    imguiInitialized = false;
    headerFont = nullptr;
    textFont = nullptr;
    smallFont = nullptr;
    featureToggles.clear();
}

void UIRenderer::RenderESP(Trainer& trainer) {
    // Get screen dimensions
    RECT clientRect;
    if (!GetClientRect(gameWindow, &clientRect)) return;
    
    int screenWidth = clientRect.right;
    int screenHeight = clientRect.bottom;

    ViewMatrix viewMatrix = {};
    trainer.GetViewMatrix(viewMatrix.data);

    // Guard against an uninitialized matrix (all zeros) which would fail the projection test.
    bool hasValidMatrix = false;
    for (float value : viewMatrix.data) {
        if (value != 0.0f) {
            hasValidMatrix = true;
            break;
        }
    }
    if (!hasValidMatrix) {
        return;
    }

    // Get all players
    std::vector<uintptr_t> players;
    if (!trainer.GetPlayerList(players)) return;
    
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    
    // Draw ESP info in top-right corner
    char infoText[128];
    snprintf(infoText, sizeof(infoText), "ESP ACTIVE: %d players", (int)players.size());
    drawList->AddText(ImVec2(screenWidth - 180.0f, 80.0f), IM_COL32(0, 255, 0, 255), infoText);

    
    int validPlayers = 0;
    int offScreenPlayers = 0;
    
    // Draw each player with 3D world-to-screen projection
    for (uintptr_t playerPtr : players) {
        bool valid = trainer.IsPlayerValid(playerPtr);
        bool alive = trainer.IsPlayerAlive(playerPtr);
        int team = trainer.GetPlayerTeam(playerPtr);
        float x, y, z;
        trainer.GetPlayerPosition(playerPtr, x, y, z);
        char name[64] = {0};
        trainer.GetPlayerName(playerPtr, name, sizeof(name));

        // Debug: show entity info for all entities
        char entityDbg[256];
        snprintf(entityDbg, sizeof(entityDbg), "Entity %s: valid=%d alive=%d team=%d pos=(%.1f,%.1f,%.1f)", name, valid, alive, team, x, y, z);
        drawList->AddText(ImVec2(20, 120 + 16 * validPlayers), IM_COL32(255,128,0,255), entityDbg);

        if (!valid || !alive) continue;
        validPlayers++;

        // Project 3D position to screen using matrix
        Vec3 pos = {x, y, z};
        float screenPos[2];
        if (!WorldToScreenMatrix(pos, screenPos, viewMatrix, screenWidth, screenHeight)) {
            offScreenPlayers++;
            continue;  // Player is off screen or behind camera
            // Draw a visible debug marker to confirm ESP rendering
            drawList->AddRectFilled(ImVec2(50, 50), ImVec2(200, 100), IM_COL32(0, 255, 255, 255));
            drawList->AddText(ImVec2(60, 60), IM_COL32(255, 0, 0, 255), "ESP DEBUG MARKER");
        }
        float screenX = screenPos[0];
        float screenY = screenPos[1];
        // Debug output for position and projection
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "Player %s: World(%.1f,%.1f,%.1f) -> Screen(%.1f,%.1f)", name, x, y, z, screenX, screenY);
        drawList->AddText(ImVec2(20, 140 + 16 * validPlayers), IM_COL32(255,255,0,255), dbg);
        // Choose color based on team (red for enemies, green for teammates)
        ImU32 boxColor = IM_COL32(255, 50, 50, 200);  // Red for enemies
        ImU32 textColor = IM_COL32(255, 255, 255, 255);  // White text
        
        // Draw box around player (feet at screenY, head above)
        float boxWidth = 40.0f;
        float boxHeight = 60.0f;
        ImVec2 topLeft(screenX - boxWidth/2, screenY - boxHeight);
        ImVec2 bottomRight(screenX + boxWidth/2, screenY);
        
        // Draw filled background with transparency
        drawList->AddRectFilled(topLeft, bottomRight, IM_COL32(0, 0, 0, 100));
        // Draw box outline
        drawList->AddRect(topLeft, bottomRight, boxColor, 0.0f, 0, 2.0f);
        
        // Draw player name above box
        if (name[0] != 0) {
            ImVec2 textSize = ImGui::CalcTextSize(name);
            ImVec2 namePos(screenX - textSize.x/2, screenY - boxHeight - 15.0f);
            
            // Draw text background
            drawList->AddRectFilled(
                ImVec2(namePos.x - 2, namePos.y - 2),
                ImVec2(namePos.x + textSize.x + 2, namePos.y + textSize.y + 2),
                IM_COL32(0, 0, 0, 150)
            );
            drawList->AddText(namePos, textColor, name);
        }
        
        // Draw distance indicator below box
        // ...existing code...
    }
    
    // Draw debug info
    char debugText[128];
    snprintf(debugText, sizeof(debugText), "Valid: %d | OffScreen: %d", validPlayers, offScreenPlayers);
    drawList->AddText(ImVec2(screenWidth - 180.0f, 100.0f), IM_COL32(200, 200, 200, 255), debugText);
}
