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

// Enhanced color scheme
constexpr ImU32 kAccentColor = IM_COL32(88, 166, 255, 255);      // Bright blue accent
constexpr ImU32 kAccentColorDim = IM_COL32(60, 120, 200, 255);   // Dimmer blue
constexpr ImU32 kSuccessColor = IM_COL32(80, 220, 120, 255);     // Green
constexpr ImU32 kWarningColor = IM_COL32(255, 180, 60, 255);     // Orange
constexpr ImU32 kDangerColor = IM_COL32(235, 80, 80, 255);       // Red

constexpr float kButtonHeight = 36.0f;
constexpr float kButtonSpacing = 10.0f;
constexpr float kIndicatorSize = 16.0f;
constexpr float kCornerRadius = 10.0f;
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

        // Enhanced styling with gradients
        ImU32 bgColorTop = isHovered ? ColorU32(50, 55, 65) : ColorU32(40, 44, 50);
        ImU32 bgColorBottom = isHovered ? ColorU32(42, 47, 55) : ColorU32(32, 36, 42);
        ImU32 borderColor = isActive ? kSuccessColor : (isHovered ? kAccentColor : ColorU32(60, 70, 85));

        // Draw gradient background
        drawList->AddRectFilledMultiColor(minVec, maxVec, bgColorTop, bgColorTop, bgColorBottom, bgColorBottom);

        // Draw border with glow effect when active
        if (isActive) {
            // Outer glow
            drawList->AddRect(
                ImVec2(minVec.x - 1, minVec.y - 1),
                ImVec2(maxVec.x + 1, maxVec.y + 1),
                IM_COL32(80, 220, 120, 100),
                kCornerRadius + 1.0f, 0, 3.0f
            );
        }
        drawList->AddRect(minVec, maxVec, borderColor, kCornerRadius, 0, isActive ? 2.5f : (isHovered ? 2.0f : 1.5f));

        // Draw modern toggle switch instead of square indicator
        float switchWidth = 40.0f;
        float switchHeight = 20.0f;
        ImVec2 switchMin(maxVec.x - switchWidth - 12.0f, minVec.y + (kButtonHeight - switchHeight) * 0.5f);
        ImVec2 switchMax(switchMin.x + switchWidth, switchMin.y + switchHeight);

        ImU32 switchBg = isActive ? kSuccessColor : ColorU32(60, 65, 75);
        drawList->AddRectFilled(switchMin, switchMax, switchBg, switchHeight * 0.5f);

        // Draw toggle circle
        float circleRadius = switchHeight * 0.4f;
        float circleX = isActive ? (switchMax.x - circleRadius - 3.0f) : (switchMin.x + circleRadius + 3.0f);
        float circleY = switchMin.y + switchHeight * 0.5f;
        drawList->AddCircleFilled(ImVec2(circleX, circleY), circleRadius, ColorU32(255, 255, 255));

        // Draw feature name
        ImVec2 textPos(minVec.x + 14.0f, minVec.y + (kButtonHeight - 16.0f) * 0.5f);
        ImU32 textColor = isActive ? ColorU32(220, 225, 230) : ColorU32(170, 175, 180);
        drawList->AddText(textFont ? textFont : ImGui::GetFont(),
                          textFont ? textFont->FontSize : ImGui::GetFontSize(),
                          textPos,
                          textColor,
                          featureToggles[i].name.c_str());

        y += kButtonHeight + kButtonSpacing;
    }

    return y + 6.0f;
}

float UIRenderer::DrawPlayerStats(ImDrawList* drawList, const ImVec2& start, const PlayerStats& stats) {
    float y = start.y;
    const float barWidth = static_cast<float>(panelWidth - panelPadding * 2);

    // Section header
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x, y),
                      kAccentColorDim,
                      "PLAYER STATS");
    y += 22.0f;

    // Helper to draw stat bar
    auto drawStatBar = [&](const char* label, int value, int maxValue, ImU32 barColor) {
        // Label
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x, y),
                          ColorU32(180, 185, 190),
                          label);

        // Value
        char valueText[16];
        sprintf_s(valueText, "%d", value);
        ImVec2 valueSize = ImGui::CalcTextSize(valueText);
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x + barWidth - valueSize.x, y),
                          ColorU32(220, 225, 230),
                          valueText);
        y += 18.0f;

        // Progress bar background
        ImVec2 barBg(start.x, y);
        ImVec2 barBgMax(start.x + barWidth, y + 6.0f);
        drawList->AddRectFilled(barBg, barBgMax, ColorU32(30, 33, 38), 3.0f);

        // Progress bar fill
        float fillPercent = static_cast<float>(value) / static_cast<float>(maxValue);
        if (fillPercent > 1.0f) fillPercent = 1.0f;
        if (fillPercent > 0.0f) {
            ImVec2 barFillMax(start.x + barWidth * fillPercent, y + 6.0f);
            drawList->AddRectFilled(barBg, barFillMax, barColor, 3.0f);
        }

        y += 12.0f;
    };

    ImU32 healthColor = stats.health > 70 ? kSuccessColor : (stats.health > 30 ? kWarningColor : kDangerColor);
    drawStatBar("Health", stats.health, 100, healthColor);

    ImU32 armorColor = stats.armor > 50 ? kAccentColor : ColorU32(120, 130, 145);
    drawStatBar("Armor", stats.armor, 100, armorColor);

    drawStatBar("Ammo", stats.ammo, 100, ColorU32(150, 160, 180));

    return y + sectionSpacing;
}

float UIRenderer::DrawAimbotSettings(ImDrawList* drawList, const ImVec2& start, Trainer& trainer) {
    if (!trainer.IsAimbotEnabled()) {
        return start.y;  // Don't show settings if aimbot is off
    }

    float y = start.y;
    const float barWidth = static_cast<float>(panelWidth - panelPadding * 2);
    ImGuiIO& io = ImGui::GetIO();

    // Section header
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x, y),
                      kWarningColor,
                      "AIMBOT SETTINGS");
    y += 22.0f;

    // Mode toggle button
    const float toggleHeight = 28.0f;
    bool useFOV = trainer.GetAimbotUseFOV();
    const char* modeText = useFOV ? "Mode: Crosshair" : "Mode: Distance";

    ImVec2 modeMin(start.x, y);
    ImVec2 modeMax(start.x + barWidth, y + toggleHeight);
    bool modeHovered = io.MousePos.x >= modeMin.x && io.MousePos.x <= modeMax.x &&
                       io.MousePos.y >= modeMin.y && io.MousePos.y <= modeMax.y;

    if (modeHovered && ImGui::IsMouseClicked(0)) {
        trainer.SetAimbotUseFOV(!useFOV);
    }

    ImU32 modeBg = modeHovered ? ColorU32(50, 55, 65) : ColorU32(40, 44, 50);
    drawList->AddRectFilled(modeMin, modeMax, modeBg, 6.0f);
    drawList->AddRect(modeMin, modeMax, useFOV ? kAccentColor : ColorU32(100, 110, 120), 6.0f, 0, 1.5f);

    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x + 8.0f, y + 7.0f),
                      ColorU32(200, 205, 210),
                      modeText);
    y += toggleHeight + 12.0f;

    // Smoothness slider
    float smoothness = trainer.GetAimbotSmoothness();
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x, y),
                      ColorU32(180, 185, 190),
                      "Smoothness");

    char smoothText[16];
    snprintf(smoothText, sizeof(smoothText), "%.1fx", smoothness);
    ImVec2 smoothSize = ImGui::CalcTextSize(smoothText);
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x + barWidth - smoothSize.x, y),
                      ColorU32(220, 225, 230),
                      smoothText);
    y += 18.0f;

    // Slider background
    const float sliderHeight = 20.0f;
    ImVec2 sliderBg(start.x, y);
    ImVec2 sliderBgMax(start.x + barWidth, y + sliderHeight);

    // Check if mouse is over slider
    bool sliderHovered = io.MousePos.x >= sliderBg.x && io.MousePos.x <= sliderBgMax.x &&
                         io.MousePos.y >= sliderBg.y && io.MousePos.y <= sliderBgMax.y;

    // Handle slider dragging
    static bool draggingSmooth = false;
    if (sliderHovered && ImGui::IsMouseDown(0)) {
        draggingSmooth = true;
    }
    if (!ImGui::IsMouseDown(0)) {
        draggingSmooth = false;
    }

    if (draggingSmooth) {
        float percent = (io.MousePos.x - sliderBg.x) / barWidth;
        if (percent < 0.0f) percent = 0.0f;
        if (percent > 1.0f) percent = 1.0f;
        float newSmooth = 1.0f + percent * 9.0f;  // Range: 1.0 to 10.0
        trainer.SetAimbotSmoothness(newSmooth);
        smoothness = newSmooth;
    }

    drawList->AddRectFilled(sliderBg, sliderBgMax, ColorU32(30, 33, 38), sliderHeight * 0.5f);

    // Slider fill
    float smoothPercent = (smoothness - 1.0f) / 9.0f;
    ImVec2 sliderFillMax(start.x + barWidth * smoothPercent, y + sliderHeight);
    drawList->AddRectFilled(sliderBg, sliderFillMax, kSuccessColor, sliderHeight * 0.5f);

    // Slider handle
    float handleX = start.x + barWidth * smoothPercent;
    drawList->AddCircleFilled(ImVec2(handleX, y + sliderHeight * 0.5f), 8.0f, ColorU32(255, 255, 255));
    y += sliderHeight + 12.0f;

    // FOV slider (only show if using FOV mode)
    if (useFOV) {
        float fov = trainer.GetAimbotFOV();
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x, y),
                          ColorU32(180, 185, 190),
                          "FOV");

        char fovText[16];
        snprintf(fovText, sizeof(fovText), "%.0f deg", fov);
        ImVec2 fovSize = ImGui::CalcTextSize(fovText);
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x + barWidth - fovSize.x, y),
                          ColorU32(220, 225, 230),
                          fovText);
        y += 18.0f;

        // FOV Slider
        ImVec2 fovSliderBg(start.x, y);
        ImVec2 fovSliderBgMax(start.x + barWidth, y + sliderHeight);

        bool fovSliderHovered = io.MousePos.x >= fovSliderBg.x && io.MousePos.x <= fovSliderBgMax.x &&
                                io.MousePos.y >= fovSliderBg.y && io.MousePos.y <= fovSliderBgMax.y;

        static bool draggingFOV = false;
        if (fovSliderHovered && ImGui::IsMouseDown(0)) {
            draggingFOV = true;
        }
        if (!ImGui::IsMouseDown(0)) {
            draggingFOV = false;
        }

        if (draggingFOV) {
            float percent = (io.MousePos.x - fovSliderBg.x) / barWidth;
            if (percent < 0.0f) percent = 0.0f;
            if (percent > 1.0f) percent = 1.0f;
            float newFOV = 10.0f + percent * 170.0f;  // Range: 10 to 180 degrees
            trainer.SetAimbotFOV(newFOV);
            fov = newFOV;
        }

        drawList->AddRectFilled(fovSliderBg, fovSliderBgMax, ColorU32(30, 33, 38), sliderHeight * 0.5f);

        float fovPercent = (fov - 10.0f) / 170.0f;
        ImVec2 fovFillMax(start.x + barWidth * fovPercent, y + sliderHeight);
        drawList->AddRectFilled(fovSliderBg, fovFillMax, kAccentColor, sliderHeight * 0.5f);

        float fovHandleX = start.x + barWidth * fovPercent;
        drawList->AddCircleFilled(ImVec2(fovHandleX, y + sliderHeight * 0.5f), 8.0f, ColorU32(255, 255, 255));
        y += sliderHeight + 12.0f;
    }

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

    // Danger button styling
    ImU32 bgColorTop = isHovered ? ColorU32(200, 70, 70) : ColorU32(160, 50, 50);
    ImU32 bgColorBottom = isHovered ? ColorU32(180, 50, 50) : ColorU32(140, 35, 35);

    // Draw gradient background
    drawList->AddRectFilledMultiColor(minVec, maxVec, bgColorTop, bgColorTop, bgColorBottom, bgColorBottom);

    // Border with warning color
    drawList->AddRect(minVec, maxVec, isHovered ? kWarningColor : kDangerColor, kCornerRadius, 0, isHovered ? 2.5f : 2.0f);

    // Icon + text
    const char* icon = "!";
    const char* text = " UNLOAD TRAINER";

    ImVec2 iconSize = ImGui::CalcTextSize(icon);
    ImVec2 textSize = ImGui::CalcTextSize(text);
    float totalWidth = iconSize.x + textSize.x;

    ImVec2 iconPos(minVec.x + (buttonWidth - totalWidth) * 0.5f, minVec.y + (kButtonHeight - iconSize.y) * 0.5f);
    ImVec2 textPos(iconPos.x + iconSize.x, minVec.y + (kButtonHeight - textSize.y) * 0.5f);

    drawList->AddText(textFont ? textFont : ImGui::GetFont(),
                      textFont ? textFont->FontSize : ImGui::GetFontSize(),
                      iconPos,
                      kWarningColor,
                      icon);

    drawList->AddText(textFont ? textFont : ImGui::GetFont(),
                      textFont ? textFont->FontSize : ImGui::GetFontSize(),
                      textPos,
                      ColorU32(255, 255, 255),
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

void UIRenderer::DrawMenu(const PlayerStats& stats, Trainer& trainer) {
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 panelPos(24.0f, 24.0f);

    const float headerHeight = 50.0f;
    const float hintHeight = 24.0f;
    const float togglesHeight = static_cast<float>(featureToggles.size()) * (kButtonHeight + kButtonSpacing) + 6.0f;
    const float statsHeight = 26.0f + (22.0f * 3.0f) + static_cast<float>(sectionSpacing);

    // Calculate aimbot settings height (only if aimbot is enabled)
    float aimbotHeight = 0.0f;
    if (trainer.IsAimbotEnabled()) {
        aimbotHeight = 22.0f + 28.0f + 12.0f + 18.0f + 20.0f + 12.0f;  // Header + mode toggle + smoothness
        if (trainer.GetAimbotUseFOV()) {
            aimbotHeight += 18.0f + 20.0f + 12.0f;  // FOV slider
        }
        aimbotHeight += static_cast<float>(sectionSpacing);
    }

    const float unloadHeight = kButtonHeight + 6.0f;

    const float panelHeight = static_cast<float>(panelPadding) * 2.0f +
                              headerHeight + hintHeight + togglesHeight + statsHeight + aimbotHeight + unloadHeight;

    ImVec2 panelMin = panelPos;
    ImVec2 panelMax(panelPos.x + static_cast<float>(panelWidth), panelPos.y + panelHeight);

    // Modern panel with shadow effect
    ImVec2 shadowOffset(4.0f, 4.0f);
    drawList->AddRectFilled(
        ImVec2(panelMin.x + shadowOffset.x, panelMin.y + shadowOffset.y),
        ImVec2(panelMax.x + shadowOffset.x, panelMax.y + shadowOffset.y),
        ColorU32(0, 0, 0, 80), 12.0f
    );

    // Panel background with gradient
    drawList->AddRectFilledMultiColor(
        panelMin, panelMax,
        ColorU32(28, 32, 38, 245), ColorU32(28, 32, 38, 245),
        ColorU32(22, 25, 30, 245), ColorU32(22, 25, 30, 245)
    );

    // Accent border
    drawList->AddRect(panelMin, panelMax, kAccentColor, 12.0f, 0, 2.5f);

    float y = panelPos.y + static_cast<float>(panelPadding);

    // Draw header with accent bar
    ImVec2 headerBarMin(panelPos.x + panelPadding, y);
    ImVec2 headerBarMax(panelPos.x + panelWidth - panelPadding, y + 3.0f);
    drawList->AddRectFilledMultiColor(
        headerBarMin, headerBarMax,
        kAccentColor, kSuccessColor,
        kSuccessColor, kAccentColor
    );
    y += 8.0f;

    // Title text
    drawList->AddText(headerFont ? headerFont : ImGui::GetFont(),
                      headerFont ? headerFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(panelPos.x + panelPadding, y),
                      ColorU32(255, 255, 255),
                      "ASSAULT CUBE");
    y += 22.0f;

    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(panelPos.x + panelPadding, y),
                      kAccentColorDim,
                      "Training Tool v2.0");
    y += 20.0f;

    // Hint text
    const char* hint = "Click to toggle | INSERT: Hide";
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(panelPos.x + panelPadding, y),
                      ColorU32(130, 135, 140),
                      hint);
    y += hintHeight;

    ImVec2 sectionStart(panelPos.x + panelPadding, y);
    y = DrawFeatureToggles(drawList, sectionStart);

    sectionStart = ImVec2(panelPos.x + panelPadding, y);
    y = DrawPlayerStats(drawList, sectionStart, stats);

    // Draw aimbot settings if aimbot is enabled
    sectionStart = ImVec2(panelPos.x + panelPadding, y);
    y = DrawAimbotSettings(drawList, sectionStart, trainer);

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

    // Draw aimbot FOV circle if aimbot is enabled and using FOV mode
    if (trainer.IsAimbotEnabled() && trainer.GetAimbotUseFOV()) {
        RECT clientRect;
        if (GetClientRect(gameWindow, &clientRect)) {
            int screenWidth = clientRect.right;
            int screenHeight = clientRect.bottom;

            float centerX = screenWidth * 0.5f;
            float centerY = screenHeight * 0.5f;

            // Calculate FOV circle radius
            // We use a simple approximation: 1 degree ≈ screenHeight / 90 pixels
            float fov = trainer.GetAimbotFOV();
            float radiusPixels = (fov / 90.0f) * (screenHeight * 0.5f);

            ImDrawList* fovDrawList = ImGui::GetForegroundDrawList();

            // Draw FOV circle outline
            fovDrawList->AddCircle(
                ImVec2(centerX, centerY),
                radiusPixels,
                IM_COL32(255, 200, 60, 150),
                64,
                2.0f
            );

            // Draw small crosshair in center
            float crosshairSize = 8.0f;
            fovDrawList->AddLine(
                ImVec2(centerX - crosshairSize, centerY),
                ImVec2(centerX + crosshairSize, centerY),
                IM_COL32(255, 200, 60, 200),
                2.0f
            );
            fovDrawList->AddLine(
                ImVec2(centerX, centerY - crosshairSize),
                ImVec2(centerX, centerY + crosshairSize),
                IM_COL32(255, 200, 60, 200),
                2.0f
            );

            // Draw FOV label
            char fovLabel[32];
            snprintf(fovLabel, sizeof(fovLabel), "FOV: %.0f deg", fov);
            fovDrawList->AddText(
                ImVec2(centerX + radiusPixels + 10.0f, centerY - 10.0f),
                IM_COL32(255, 200, 60, 255),
                fovLabel
            );
        }
    }

    if (menuVisible) {
        featureToggles = trainer.BuildFeatureToggles();
        PlayerStats stats = trainer.GetPlayerStats();
        UpdateNavigation(trainer);
        DrawMenu(stats, trainer);
        
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
    // ALWAYS draw debug marker first to confirm ESP is being called
    ImDrawList* debugDrawList = ImGui::GetForegroundDrawList();
    debugDrawList->AddRectFilled(ImVec2(10, 200), ImVec2(210, 240), IM_COL32(255, 0, 255, 200));
    debugDrawList->AddText(ImVec2(15, 205), IM_COL32(255, 255, 255, 255), "ESP FUNCTION CALLED");

    // Get screen dimensions
    RECT clientRect;
    if (!GetClientRect(gameWindow, &clientRect)) {
        debugDrawList->AddText(ImVec2(15, 220), IM_COL32(255, 0, 0, 255), "GetClientRect FAILED");
        return;
    }

    int screenWidth = clientRect.right;
    int screenHeight = clientRect.bottom;

    // Get combined view-projection matrix (modelview * projection)
    ViewMatrix viewProjMatrix = {};
    trainer.GetViewProjectionMatrix(viewProjMatrix.data);

    // Guard against an uninitialized matrix (all zeros) which would fail the projection test.
    bool hasValidMatrix = false;
    for (float value : viewProjMatrix.data) {
        if (value != 0.0f) {
            hasValidMatrix = true;
            break;
        }
    }

    // DEBUG: Show first few matrix values
    char matrixDebug[256];
    snprintf(matrixDebug, sizeof(matrixDebug), "ViewProj[0-3]: %.2f %.2f %.2f %.2f",
             viewProjMatrix.data[0], viewProjMatrix.data[1], viewProjMatrix.data[2], viewProjMatrix.data[3]);
    debugDrawList->AddText(ImVec2(15, 250), IM_COL32(200, 200, 255, 255), matrixDebug);

    if (!hasValidMatrix) {
        debugDrawList->AddText(ImVec2(15, 220), IM_COL32(255, 0, 0, 255), "MATRIX IS ALL ZEROS");
        return;
    }

    // Get all players
    std::vector<uintptr_t> players;
    if (!trainer.GetPlayerList(players)) {
        debugDrawList->AddText(ImVec2(15, 220), IM_COL32(255, 0, 0, 255), "GetPlayerList FAILED");
        return;
    }

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

        // Project 3D position to screen using combined view-projection matrix
        Vec3 pos = {x, y, z};
        float screenPos[2];
        if (!WorldToScreenMatrix(pos, screenPos, viewProjMatrix, screenWidth, screenHeight)) {
            offScreenPlayers++;
            continue;  // Player is off screen or behind camera
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
