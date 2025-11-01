#include "pch.h"
#include "ui.h"
#include "trainer.h"
#include "memory.h"
#include "render_utils.h"
#include "gl_hook.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include <cstdio>
#include <windowsx.h>
#include <gl/GL.h>

using RenderUtils::Distance3D;
using RenderUtils::Draw2DBox;
using RenderUtils::Draw3DBox;
using RenderUtils::DrawSnapline;
using RenderUtils::TransformContext;
using RenderUtils::Vec3;
using RenderUtils::WorldToScreen;

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
//ui render class constructor, sets all values to its standard intialization.
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
      isDragging(false),
      panelPosX(24.0f),
      panelPosY(24.0f),
      dragOffsetX(0.0f),
      dragOffsetY(0.0f),
      currentTab(UITab::MISC),
      headerFont(nullptr),
      textFont(nullptr),
      smallFont(nullptr) {
    savedClipRect = {0, 0, 0, 0};
}
// Destructor
UIRenderer::~UIRenderer() {
    Shutdown();
}
// initializes the ui render to the target window
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
//initializes imgui
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

    // Filter out ESP and Aimbot toggles - they should be in their own tabs
    std::vector<size_t> filteredIndices;
    for (size_t i = 0; i < featureToggles.size(); ++i) {
        const std::string& name = featureToggles[i].name;
        if (name != "ESP / Wallhack" && name != "Aimbot") {
            filteredIndices.push_back(i);
        }
    }

    for (size_t idx : filteredIndices) {
        const bool isActive = featureToggles[idx].isActive ? featureToggles[idx].isActive() : false;

        ImVec2 minVec(start.x, y);
        ImVec2 maxVec(start.x + buttonWidth, y + kButtonHeight);

        // Check if mouse is hovering over this button
        bool isHovered = io.MousePos.x >= minVec.x && io.MousePos.x <= maxVec.x &&
                        io.MousePos.y >= minVec.y && io.MousePos.y <= maxVec.y;

        // Check for click
        if (isHovered && ImGui::IsMouseClicked(0)) {
            if (featureToggles[idx].onToggle) {
                featureToggles[idx].onToggle();
            }
        }

        // Enhanced styling with gradients
        ImU32 bgColorTop = isHovered ? ColorU32(50, 55, 65) : ColorU32(40, 44, 50);
        ImU32 bgColorBottom = isHovered ? ColorU32(42, 47, 55) : ColorU32(32, 36, 42);
        ImU32 borderColor = isActive ? kSuccessColor : (isHovered ? kAccentColor : ColorU32(60, 70, 85));

        // Draw gradient background with rounded corners
        drawList->AddRectFilled(minVec, maxVec, bgColorTop, kCornerRadius);

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
                          featureToggles[idx].name.c_str());

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

    // Ignore Walls toggle button
    bool ignoreWalls = trainer.GetAimbotIgnoreWalls();
    const char* wallText = ignoreWalls ? "Walls: IGNORED" : "Walls: Blocked";

    ImVec2 wallMin(start.x, y);
    ImVec2 wallMax(start.x + barWidth, y + toggleHeight);
    bool wallHovered = io.MousePos.x >= wallMin.x && io.MousePos.x <= wallMax.x &&
                       io.MousePos.y >= wallMin.y && io.MousePos.y <= wallMax.y;

    if (wallHovered && ImGui::IsMouseClicked(0)) {
        trainer.SetAimbotIgnoreWalls(!ignoreWalls);
    }

    ImU32 wallBg = wallHovered ? ColorU32(50, 55, 65) : ColorU32(40, 44, 50);
    drawList->AddRectFilled(wallMin, wallMax, wallBg, 6.0f);
    drawList->AddRect(wallMin, wallMax, ignoreWalls ? kWarningColor : kSuccessColor, 6.0f, 0, 1.5f);

    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x + 8.0f, y + 7.0f),
                      ignoreWalls ? ColorU32(255, 200, 100) : ColorU32(100, 255, 150),
                      wallText);
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

    // Triggerbot section header
    y += 10.0f;
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x, y),
                      kWarningColor,
                      "TRIGGERBOT");
    y += 22.0f;

    // Triggerbot enable toggle
    bool triggerbotEnabled = trainer.IsTriggerbotEnabled();
    const char* triggerText = triggerbotEnabled ? "Triggerbot: ON" : "Triggerbot: OFF";

    ImVec2 triggerMin(start.x, y);
    ImVec2 triggerMax(start.x + barWidth, y + 28.0f);
    bool triggerHovered = io.MousePos.x >= triggerMin.x && io.MousePos.x <= triggerMax.x &&
                          io.MousePos.y >= triggerMin.y && io.MousePos.y <= triggerMax.y;

    if (triggerHovered && ImGui::IsMouseClicked(0)) {
        trainer.SetTriggerbotEnabled(!triggerbotEnabled);
    }

    ImU32 triggerBg = triggerHovered ? ColorU32(50, 55, 65) : ColorU32(40, 44, 50);
    drawList->AddRectFilled(triggerMin, triggerMax, triggerBg, 6.0f);
    drawList->AddRect(triggerMin, triggerMax, triggerbotEnabled ? kSuccessColor : ColorU32(100, 110, 120), 6.0f, 0, 1.5f);

    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x + 8.0f, y + 7.0f),
                      triggerbotEnabled ? ColorU32(100, 255, 150) : ColorU32(200, 205, 210),
                      triggerText);
    y += 28.0f + 12.0f;

    // Triggerbot settings (only show if enabled)
    if (triggerbotEnabled) {
        // Delay slider
        float delay = trainer.GetTriggerbotDelay();
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x, y),
                          ColorU32(180, 185, 190),
                          "Delay");

        char delayText[16];
        snprintf(delayText, sizeof(delayText), "%.0fms", delay);
        ImVec2 delaySize = ImGui::CalcTextSize(delayText);
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x + barWidth - delaySize.x, y),
                          ColorU32(220, 225, 230),
                          delayText);
        y += 18.0f;

        // Delay slider
        const float sliderHeight = 20.0f;
        ImVec2 delaySliderBg(start.x, y);
        ImVec2 delaySliderBgMax(start.x + barWidth, y + sliderHeight);

        bool delaySliderHovered = io.MousePos.x >= delaySliderBg.x && io.MousePos.x <= delaySliderBgMax.x &&
                                  io.MousePos.y >= delaySliderBg.y && io.MousePos.y <= delaySliderBgMax.y;

        static bool draggingDelay = false;
        if (delaySliderHovered && ImGui::IsMouseDown(0)) {
            draggingDelay = true;
        }
        if (!ImGui::IsMouseDown(0)) {
            draggingDelay = false;
        }

        if (draggingDelay) {
            float percent = (io.MousePos.x - delaySliderBg.x) / barWidth;
            if (percent < 0.0f) percent = 0.0f;
            if (percent > 1.0f) percent = 1.0f;
            float newDelay = 0.0f + percent * 300.0f;  // Range: 0 to 300ms
            trainer.SetTriggerbotDelay(newDelay);
            delay = newDelay;
        }

        drawList->AddRectFilled(delaySliderBg, delaySliderBgMax, ColorU32(30, 33, 38), sliderHeight * 0.5f);

        float delayPercent = delay / 300.0f;
        ImVec2 delayFillMax(start.x + barWidth * delayPercent, y + sliderHeight);
        drawList->AddRectFilled(delaySliderBg, delayFillMax, kWarningColor, sliderHeight * 0.5f);

        float delayHandleX = start.x + barWidth * delayPercent;
        drawList->AddCircleFilled(ImVec2(delayHandleX, y + sliderHeight * 0.5f), 8.0f, ColorU32(255, 255, 255));
        y += sliderHeight + 12.0f;

        // FOV tolerance slider
        float triggerFOV = trainer.GetTriggerbotFOV();
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x, y),
                          ColorU32(180, 185, 190),
                          "FOV Tolerance");

        char triggerFOVText[16];
        snprintf(triggerFOVText, sizeof(triggerFOVText), "%.1f deg", triggerFOV);
        ImVec2 triggerFOVSize = ImGui::CalcTextSize(triggerFOVText);
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x + barWidth - triggerFOVSize.x, y),
                          ColorU32(220, 225, 230),
                          triggerFOVText);
        y += 18.0f;

        // FOV tolerance slider
        ImVec2 triggerFOVSliderBg(start.x, y);
        ImVec2 triggerFOVSliderBgMax(start.x + barWidth, y + sliderHeight);

        bool triggerFOVSliderHovered = io.MousePos.x >= triggerFOVSliderBg.x && io.MousePos.x <= triggerFOVSliderBgMax.x &&
                                       io.MousePos.y >= triggerFOVSliderBg.y && io.MousePos.y <= triggerFOVSliderBgMax.y;

        static bool draggingTriggerFOV = false;
        if (triggerFOVSliderHovered && ImGui::IsMouseDown(0)) {
            draggingTriggerFOV = true;
        }
        if (!ImGui::IsMouseDown(0)) {
            draggingTriggerFOV = false;
        }

        if (draggingTriggerFOV) {
            float percent = (io.MousePos.x - triggerFOVSliderBg.x) / barWidth;
            if (percent < 0.0f) percent = 0.0f;
            if (percent > 1.0f) percent = 1.0f;
            float newTriggerFOV = 0.5f + percent * 9.5f;  // Range: 0.5 to 10 degrees
            trainer.SetTriggerbotFOV(newTriggerFOV);
            triggerFOV = newTriggerFOV;
        }

        drawList->AddRectFilled(triggerFOVSliderBg, triggerFOVSliderBgMax, ColorU32(30, 33, 38), sliderHeight * 0.5f);

        float triggerFOVPercent = (triggerFOV - 0.5f) / 9.5f;
        ImVec2 triggerFOVFillMax(start.x + barWidth * triggerFOVPercent, y + sliderHeight);
        drawList->AddRectFilled(triggerFOVSliderBg, triggerFOVFillMax, kWarningColor, sliderHeight * 0.5f);

        float triggerFOVHandleX = start.x + barWidth * triggerFOVPercent;
        drawList->AddCircleFilled(ImVec2(triggerFOVHandleX, y + sliderHeight * 0.5f), 8.0f, ColorU32(255, 255, 255));
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

    // Danger button styling - use solid color with rounded corners
    ImU32 bgColor = isHovered ? ColorU32(200, 70, 70) : ColorU32(160, 50, 50);

    // Draw solid background with rounded corners (fixes glitchy artifacts)
    drawList->AddRectFilled(minVec, maxVec, bgColor, kCornerRadius);

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

float UIRenderer::DrawTabBar(ImDrawList* drawList, const ImVec2& start, float width) {
    ImGuiIO& io = ImGui::GetIO();
    const float tabHeight = 32.0f;
    const float tabWidth = width / 3.0f;
    float y = start.y;

    struct TabInfo {
        UITab tab;
        const char* name;
        const char* icon;
    };

    TabInfo tabs[] = {
        {UITab::MISC, "MISC", "[M]"},
        {UITab::AIMBOT, "AIMBOT", "[A]"},
        {UITab::ESP, "ESP", "[E]"}
    };

    for (int i = 0; i < 3; ++i) {
        float tabX = start.x + (i * tabWidth);
        ImVec2 tabMin(tabX, y);
        ImVec2 tabMax(tabX + tabWidth, y + tabHeight);

        bool isActive = (currentTab == tabs[i].tab);
        bool isHovered = io.MousePos.x >= tabMin.x && io.MousePos.x <= tabMax.x &&
                         io.MousePos.y >= tabMin.y && io.MousePos.y <= tabMax.y;

        // Handle tab click
        if (isHovered && ImGui::IsMouseClicked(0)) {
            currentTab = tabs[i].tab;
        }

        // Draw tab background
        ImU32 tabBg;
        if (isActive) {
            tabBg = ColorU32(35, 40, 48);  // Darker for active tab
        } else if (isHovered) {
            tabBg = ColorU32(45, 50, 58);  // Lighter on hover
        } else {
            tabBg = ColorU32(40, 45, 52);  // Normal
        }

        drawList->AddRectFilled(tabMin, tabMax, tabBg);

        // Draw tab border
        if (isActive) {
            // Active tab gets a top accent line
            drawList->AddLine(
                ImVec2(tabMin.x, tabMin.y),
                ImVec2(tabMax.x, tabMin.y),
                kAccentColor,
                3.0f
            );
        }

        // Draw vertical separators between tabs
        if (i < 2) {
            drawList->AddLine(
                ImVec2(tabMax.x, tabMin.y),
                ImVec2(tabMax.x, tabMax.y),
                ColorU32(60, 65, 75, 100),
                1.0f
            );
        }

        // Draw tab text
        char tabText[32];
        snprintf(tabText, sizeof(tabText), "%s %s", tabs[i].icon, tabs[i].name);
        ImVec2 textSize = ImGui::CalcTextSize(tabText);
        ImVec2 textPos(
            tabX + (tabWidth - textSize.x) * 0.5f,
            y + (tabHeight - textSize.y) * 0.5f
        );

        ImU32 textColor = isActive ? kAccentColor : (isHovered ? ColorU32(200, 205, 210) : ColorU32(150, 155, 160));
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          textPos,
                          textColor,
                          tabText);
    }

    return y + tabHeight;
}

float UIRenderer::DrawMiscTab(ImDrawList* drawList, const ImVec2& start, Trainer& trainer, const PlayerStats& stats) {
    float y = start.y;

    // Draw section header
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x, y),
                      kAccentColorDim,
                      "GENERAL FEATURES");
    y += 22.0f;

    // Draw feature toggles (filtered for misc features)
    ImVec2 toggleStart(start.x, y);
    y = DrawFeatureToggles(drawList, toggleStart);

    // Draw player stats
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x, y),
                      kAccentColorDim,
                      "PLAYER STATS");
    y += 22.0f;

    ImVec2 statsStart(start.x, y);
    y = DrawPlayerStats(drawList, statsStart, stats);

    return y;
}

float UIRenderer::DrawAimbotTab(ImDrawList* drawList, const ImVec2& start, Trainer& trainer) {
    float y = start.y;
    ImGuiIO& io = ImGui::GetIO();
    const float buttonWidth = static_cast<float>(panelWidth - panelPadding * 2);

    // Find the aimbot toggle
    int aimbotToggleIdx = -1;
    for (size_t i = 0; i < featureToggles.size(); ++i) {
        if (featureToggles[i].name == "Aimbot") {
            aimbotToggleIdx = static_cast<int>(i);
            break;
        }
    }

    // Draw enable/disable toggle
    if (aimbotToggleIdx >= 0) {
        const bool isActive = featureToggles[aimbotToggleIdx].isActive();
        ImVec2 minVec(start.x, y);
        ImVec2 maxVec(start.x + buttonWidth, y + kButtonHeight);

        bool isHovered = io.MousePos.x >= minVec.x && io.MousePos.x <= maxVec.x &&
                        io.MousePos.y >= minVec.y && io.MousePos.y <= maxVec.y;

        if (isHovered && ImGui::IsMouseClicked(0)) {
            if (featureToggles[aimbotToggleIdx].onToggle) {
                featureToggles[aimbotToggleIdx].onToggle();
            }
        }

        // Draw button background
        ImU32 bgColor = isHovered ? ColorU32(50, 55, 65) : ColorU32(40, 44, 50);
        ImU32 borderColor = isActive ? kSuccessColor : (isHovered ? kAccentColor : ColorU32(60, 70, 85));

        drawList->AddRectFilled(minVec, maxVec, bgColor, kCornerRadius);

        if (isActive) {
            drawList->AddRect(
                ImVec2(minVec.x - 1, minVec.y - 1),
                ImVec2(maxVec.x + 1, maxVec.y + 1),
                IM_COL32(80, 220, 120, 100),
                kCornerRadius + 1.0f, 0, 3.0f
            );
        }
        drawList->AddRect(minVec, maxVec, borderColor, kCornerRadius, 0, isActive ? 2.5f : (isHovered ? 2.0f : 1.5f));

        // Draw toggle switch
        float switchWidth = 40.0f;
        float switchHeight = 20.0f;
        ImVec2 switchMin(maxVec.x - switchWidth - 12.0f, minVec.y + (kButtonHeight - switchHeight) * 0.5f);
        ImVec2 switchMax(switchMin.x + switchWidth, switchMin.y + switchHeight);
        ImU32 switchBg = isActive ? kSuccessColor : ColorU32(60, 65, 75);
        drawList->AddRectFilled(switchMin, switchMax, switchBg, switchHeight * 0.5f);

        float circleRadius = switchHeight * 0.4f;
        float circleX = isActive ? (switchMax.x - circleRadius - 3.0f) : (switchMin.x + circleRadius + 3.0f);
        float circleY = switchMin.y + switchHeight * 0.5f;
        drawList->AddCircleFilled(ImVec2(circleX, circleY), circleRadius, ColorU32(255, 255, 255));

        // Draw name
        ImVec2 textPos(minVec.x + 14.0f, minVec.y + (kButtonHeight - 16.0f) * 0.5f);
        ImU32 textColor = isActive ? ColorU32(220, 225, 230) : ColorU32(170, 175, 180);
        drawList->AddText(textFont ? textFont : ImGui::GetFont(),
                          textFont ? textFont->FontSize : ImGui::GetFontSize(),
                          textPos,
                          textColor,
                          "Enable Aimbot");

        y += kButtonHeight + 20.0f;
    }

    // Check if aimbot is enabled
    if (!trainer.IsAimbotEnabled()) {
        const char* msg = "Enable aimbot to configure settings";
        ImVec2 msgSize = ImGui::CalcTextSize(msg);
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x + (panelWidth - panelPadding * 2 - msgSize.x) * 0.5f, y + 20.0f),
                          ColorU32(150, 155, 160),
                          msg);
        return y + 80.0f;
    }

    // Draw aimbot settings
    ImVec2 settingsStart(start.x, y);
    y = DrawAimbotSettings(drawList, settingsStart, trainer);

    return y;
}

float UIRenderer::DrawESPTab(ImDrawList* drawList, const ImVec2& start, Trainer& trainer) {
    float y = start.y;
    ImGuiIO& io = ImGui::GetIO();
    const float buttonWidth = static_cast<float>(panelWidth - panelPadding * 2);

    // Find the ESP toggle
    int espToggleIdx = -1;
    for (size_t i = 0; i < featureToggles.size(); ++i) {
        if (featureToggles[i].name == "ESP / Wallhack") {
            espToggleIdx = static_cast<int>(i);
            break;
        }
    }

    // Draw enable/disable toggle
    if (espToggleIdx >= 0) {
        const bool isActive = featureToggles[espToggleIdx].isActive();
        ImVec2 minVec(start.x, y);
        ImVec2 maxVec(start.x + buttonWidth, y + kButtonHeight);

        bool isHovered = io.MousePos.x >= minVec.x && io.MousePos.x <= maxVec.x &&
                        io.MousePos.y >= minVec.y && io.MousePos.y <= maxVec.y;

        if (isHovered && ImGui::IsMouseClicked(0)) {
            if (featureToggles[espToggleIdx].onToggle) {
                featureToggles[espToggleIdx].onToggle();
            }
        }

        // Draw button background
        ImU32 bgColor = isHovered ? ColorU32(50, 55, 65) : ColorU32(40, 44, 50);
        ImU32 borderColor = isActive ? kSuccessColor : (isHovered ? kAccentColor : ColorU32(60, 70, 85));

        drawList->AddRectFilled(minVec, maxVec, bgColor, kCornerRadius);

        if (isActive) {
            drawList->AddRect(
                ImVec2(minVec.x - 1, minVec.y - 1),
                ImVec2(maxVec.x + 1, maxVec.y + 1),
                IM_COL32(80, 220, 120, 100),
                kCornerRadius + 1.0f, 0, 3.0f
            );
        }
        drawList->AddRect(minVec, maxVec, borderColor, kCornerRadius, 0, isActive ? 2.5f : (isHovered ? 2.0f : 1.5f));

        // Draw toggle switch
        float switchWidth = 40.0f;
        float switchHeight = 20.0f;
        ImVec2 switchMin(maxVec.x - switchWidth - 12.0f, minVec.y + (kButtonHeight - switchHeight) * 0.5f);
        ImVec2 switchMax(switchMin.x + switchWidth, switchMin.y + switchHeight);
        ImU32 switchBg = isActive ? kSuccessColor : ColorU32(60, 65, 75);
        drawList->AddRectFilled(switchMin, switchMax, switchBg, switchHeight * 0.5f);

        float circleRadius = switchHeight * 0.4f;
        float circleX = isActive ? (switchMax.x - circleRadius - 3.0f) : (switchMin.x + circleRadius + 3.0f);
        float circleY = switchMin.y + switchHeight * 0.5f;
        drawList->AddCircleFilled(ImVec2(circleX, circleY), circleRadius, ColorU32(255, 255, 255));

        // Draw name
        ImVec2 textPos(minVec.x + 14.0f, minVec.y + (kButtonHeight - 16.0f) * 0.5f);
        ImU32 textColor = isActive ? ColorU32(220, 225, 230) : ColorU32(170, 175, 180);
        drawList->AddText(textFont ? textFont : ImGui::GetFont(),
                          textFont ? textFont->FontSize : ImGui::GetFontSize(),
                          textPos,
                          textColor,
                          "Enable ESP");

        y += kButtonHeight + 20.0f;
    }

    bool espEnabled = trainer.IsESPEnabled();

    if (!espEnabled) {
        const char* msg = "Enable ESP to see players through walls";
        ImVec2 msgSize = ImGui::CalcTextSize(msg);
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x + (panelWidth - panelPadding * 2 - msgSize.x) * 0.5f, y + 20.0f),
                          ColorU32(150, 155, 160),
                          msg);
        return y + 80.0f;
    }

    // ESP info
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x, y),
                      ColorU32(180, 185, 190),
                      "ESP Features:");
    y += 20.0f;

    const char* features[] = {
        "- 2D Player Boxes",
        "- Player Names",
        "- Distance Indicators",
        "- Snaplines to Players",
        "- Color-Coded Distance"
    };

    for (const char* feature : features) {
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          ImVec2(start.x + 10.0f, y),
                          ColorU32(160, 165, 170),
                          feature);
        y += 18.0f;
    }

    y += 10.0f;

    // Note about ESP
    const char* note = "Note: ESP overlays are rendered in-game";
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(start.x, y),
                      kAccentColorDim,
                      note);
    y += 20.0f;

    return y;
}

void UIRenderer::DrawMenu(const PlayerStats& stats, Trainer& trainer) {
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();

    const float headerHeight = 50.0f;
    const float hintHeight = 24.0f;
    const float tabBarHeight = 32.0f;
    const float unloadHeight = kButtonHeight + 6.0f;

    // Calculate content height based on active tab
    float contentHeight = 0.0f;
    switch (currentTab) {
        case UITab::MISC: {
            // Count only MISC toggles (exclude ESP and Aimbot)
            int miscToggleCount = 0;
            for (const auto& toggle : featureToggles) {
                if (toggle.name != "ESP / Wallhack" && toggle.name != "Aimbot") {
                    miscToggleCount++;
                }
            }
            const float togglesHeight = static_cast<float>(miscToggleCount) * (kButtonHeight + kButtonSpacing) + 6.0f;
            const float statsHeight = 26.0f + (22.0f * 3.0f) + static_cast<float>(sectionSpacing);
            contentHeight = 22.0f + togglesHeight + 22.0f + statsHeight;
            break;
        }
        case UITab::AIMBOT: {
            // Toggle button + settings or message
            contentHeight = kButtonHeight + 20.0f;  // Toggle button
            if (trainer.IsAimbotEnabled()) {
                contentHeight += 22.0f + 28.0f + 12.0f + 18.0f + 20.0f + 12.0f;  // Header + mode toggle + smoothness
                if (trainer.GetAimbotUseFOV()) {
                    contentHeight += 18.0f + 20.0f + 12.0f;  // FOV slider
                }
                contentHeight += static_cast<float>(sectionSpacing);
            } else {
                contentHeight += 100.0f;  // Message height
            }
            break;
        }
        case UITab::ESP: {
            // Toggle button + info or message
            contentHeight = kButtonHeight + 20.0f;  // Toggle button
            if (trainer.IsESPEnabled()) {
                contentHeight += 200.0f;  // ESP info display
            } else {
                contentHeight += 100.0f;  // Message height
            }
            break;
        }
    }

    const float panelHeight = static_cast<float>(panelPadding) * 2.0f +
                              headerHeight + hintHeight + tabBarHeight + contentHeight + unloadHeight;

    // Handle drag-and-drop BEFORE drawing (so position updates immediately)
    ImVec2 panelPos(panelPosX, panelPosY);
    ImVec2 panelMin = panelPos;
    ImVec2 panelMax(panelPos.x + static_cast<float>(panelWidth), panelPos.y + panelHeight);

    const float dragAreaHeight = headerHeight;
    ImVec2 dragAreaMin = panelMin;
    ImVec2 dragAreaMax(panelMax.x, panelMin.y + dragAreaHeight);

    bool isDragAreaHovered = io.MousePos.x >= dragAreaMin.x && io.MousePos.x <= dragAreaMax.x &&
                             io.MousePos.y >= dragAreaMin.y && io.MousePos.y <= dragAreaMax.y;

    // Start dragging when mouse is clicked in the header area
    if (isDragAreaHovered && ImGui::IsMouseClicked(0)) {
        isDragging = true;
        dragOffsetX = io.MousePos.x - panelPosX;
        dragOffsetY = io.MousePos.y - panelPosY;
    }

    // Stop dragging when mouse is released
    if (!ImGui::IsMouseDown(0)) {
        isDragging = false;
    }

    // Update panel position while dragging
    if (isDragging) {
        panelPosX = io.MousePos.x - dragOffsetX;
        panelPosY = io.MousePos.y - dragOffsetY;

        // Clamp to screen bounds
        RECT clientRect;
        if (GetClientRect(gameWindow, &clientRect)) {
            float screenWidth = static_cast<float>(clientRect.right);
            float screenHeight = static_cast<float>(clientRect.bottom);

            // Keep at least 50px of the panel visible
            const float minVisible = 50.0f;
            if (panelPosX < -panelWidth + minVisible) panelPosX = -panelWidth + minVisible;
            if (panelPosX > screenWidth - minVisible) panelPosX = screenWidth - minVisible;
            if (panelPosY < 0.0f) panelPosY = 0.0f;
            if (panelPosY > screenHeight - minVisible) panelPosY = screenHeight - minVisible;
        }

        // Recalculate positions after drag
        panelPos = ImVec2(panelPosX, panelPosY);
        panelMin = panelPos;
        panelMax = ImVec2(panelPos.x + static_cast<float>(panelWidth), panelPos.y + panelHeight);
        dragAreaMin = panelMin;
        dragAreaMax = ImVec2(panelMax.x, panelMin.y + dragAreaHeight);
    }

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

    // Visual feedback: draw a drag cursor indicator when hovering over the header
    if (isDragAreaHovered || isDragging) {
        // Draw a subtle grab cursor hint
        const char* dragHint = isDragging ? "[ DRAGGING ]" : "[ Drag Here ]";
        ImVec2 hintSize = ImGui::CalcTextSize(dragHint);
        ImVec2 hintPos(panelPos.x + (panelWidth - hintSize.x) * 0.5f, panelPos.y + 5.0f);
        drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                          smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                          hintPos,
                          isDragging ? kSuccessColor : kAccentColorDim,
                          dragHint);
    }

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
    const char* hint = "Click tabs to switch | INSERT: Hide";
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(),
                      smallFont ? smallFont->FontSize : ImGui::GetFontSize(),
                      ImVec2(panelPos.x + panelPadding, y),
                      ColorU32(130, 135, 140),
                      hint);
    y += hintHeight;

    // Draw tab bar
    ImVec2 tabBarStart(panelPos.x, y);
    y = DrawTabBar(drawList, tabBarStart, static_cast<float>(panelWidth));
    y += 10.0f;  // Add some spacing after tab bar

    // Draw content based on active tab
    ImVec2 contentStart(panelPos.x + panelPadding, y);
    switch (currentTab) {
        case UITab::MISC:
            y = DrawMiscTab(drawList, contentStart, trainer, stats);
            break;
        case UITab::AIMBOT:
            y = DrawAimbotTab(drawList, contentStart, trainer);
            break;
        case UITab::ESP:
            y = DrawESPTab(drawList, contentStart, trainer);
            break;
    }

    y += 10.0f;  // Add spacing before unload button

    // Draw unload button at bottom
    ImVec2 sectionStart(panelPos.x + panelPadding, y);
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
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Get screen dimensions
    RECT clientRect;
    if (!GetClientRect(gameWindow, &clientRect)) return;

    int screenWidth = clientRect.right;
    int screenHeight = clientRect.bottom;

    TransformContext transform{};
    float viewMatrixF[16] = {0.0f};
    bool hasCapturedView = GetCapturedViewMatrix(viewMatrixF);
    if (!hasCapturedView) {
        trainer.GetViewMatrix(viewMatrixF);
    }

    bool viewValid = false;
    for (float value : viewMatrixF) {
        if (value != 0.0f) {
            viewValid = true;
            break;
        }
    }

    if (!viewValid) {
        for (int i = 0; i < 16; ++i) {
            viewMatrixF[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        }
    }

    double modelviewCopy[16];
    for (int i = 0; i < 16; ++i) {
        modelviewCopy[i] = static_cast<double>(viewMatrixF[i]);
    }

    float projectionMatrixF[16] = {0.0f};
    trainer.GetProjectionMatrix(projectionMatrixF);
    bool projectionValid = false;
    for (float value : projectionMatrixF) {
        if (value != 0.0f) {
            projectionValid = true;
            break;
        }
    }

    bool usingCombinedMatrix = false;
    if (projectionValid) {
        usingCombinedMatrix = true;
        for (int i = 0; i < 16; ++i) {
            transform.projection[i] = static_cast<double>(projectionMatrixF[i]);
        }
    } else {
        for (int i = 0; i < 16; ++i) {
            transform.projection[i] = (i % 5 == 0) ? 1.0 : 0.0;
        }
    }

    if (usingCombinedMatrix) {
        for (int i = 0; i < 16; ++i) {
            transform.modelview[i] = (i % 5 == 0) ? 1.0 : 0.0;
        }
    } else {
        for (int i = 0; i < 16; ++i) {
            transform.modelview[i] = modelviewCopy[i];
        }
    }

    transform.viewport[0] = 0;
    transform.viewport[1] = 0;
    transform.viewport[2] = screenWidth;
    transform.viewport[3] = screenHeight;

    // ===== DEBUG PANEL - TOP LEFT (below HOOK ACTIVE box) =====
    int debugY = 70;  // Start below the HOOK ACTIVE box (which ends at Y=60)
    const int debugLineHeight = 16;

    drawList->AddRectFilled(ImVec2(5, debugY - 2), ImVec2(400, debugY + 150), IM_COL32(0, 0, 0, 180));
    drawList->AddText(ImVec2(10, debugY), IM_COL32(255, 0, 255, 255), "=== ESP DEBUG (Camera Matrices) ===");
    debugY += debugLineHeight;

    const char* viewSource = hasCapturedView ? "View source: glLoadMatrixf hook" : "View source: memory fallback";
    drawList->AddText(ImVec2(10, debugY), IM_COL32(220, 220, 255, 255), viewSource);
    debugY += debugLineHeight;

    char matrixDebug[256];
    snprintf(matrixDebug, sizeof(matrixDebug), "ModelView[0,5,10,15]=%.2f %.2f %.2f %.2f",
             viewMatrixF[0], viewMatrixF[5], viewMatrixF[10], viewMatrixF[15]);
    drawList->AddText(ImVec2(10, debugY), IM_COL32(200, 200, 255, 255), matrixDebug);
    debugY += debugLineHeight;

    snprintf(matrixDebug, sizeof(matrixDebug), "Projection source: %s",
             projectionValid ? "Combined VP (0x0057DFD0)" : "Identity fallback");
    drawList->AddText(ImVec2(10, debugY), IM_COL32(200, 200, 255, 255), matrixDebug);
    debugY += debugLineHeight;

    char screenInfo[128];
    snprintf(screenInfo, sizeof(screenInfo), "Screen: %dx%d (from RECT)", screenWidth, screenHeight);
    drawList->AddText(ImVec2(10, debugY), IM_COL32(150, 150, 255, 255), screenInfo);
    debugY += debugLineHeight;

    // Get all players
    std::vector<uintptr_t> players;
    if (!trainer.GetPlayerList(players)) {
        drawList->AddText(ImVec2(10, debugY), IM_COL32(255, 0, 0, 255), "ERROR: GetPlayerList failed!");
        return;
    }

    char playerCount[128];
    snprintf(playerCount, sizeof(playerCount), "Player Count: %d", (int)players.size());
    drawList->AddText(ImVec2(10, debugY), IM_COL32(0, 255, 0, 255), playerCount);
    debugY += debugLineHeight;

    // Get local player position for distance calculations
    float localX, localY, localZ;
    trainer.GetLocalPlayerPosition(localX, localY, localZ);
    Vec3 localPos = {localX, localY, localZ};

    int validPlayers = 0;
    int offScreenPlayers = 0;
    int playerDebugLine = 0;

    // ===== SETUP OPENGL STATE FOR 2D RENDERING =====
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, screenWidth, screenHeight, 0, -1, 1);  // 2D orthographic projection
    glDisable(GL_DEPTH_TEST);

    // Draw each player using OpenGL
    for (uintptr_t playerPtr : players) {
        bool valid = trainer.IsPlayerValid(playerPtr);
        bool alive = trainer.IsPlayerAlive(playerPtr);
        int team = trainer.GetPlayerTeam(playerPtr);

        // Get feet and head positions
        float feetX, feetY, feetZ;
        float headX, headY, headZ;
        trainer.GetPlayerPosition(playerPtr, feetX, feetY, feetZ);
        trainer.GetPlayerHeadPosition(playerPtr, headX, headY, headZ);

        char name[64] = {0};
        trainer.GetPlayerName(playerPtr, name, sizeof(name));

        if (!valid || !alive) continue;
        validPlayers++;

        // Create position vectors
        Vec3 feetPos = {feetX, feetY, feetZ};
        Vec3 headPos = {headX, headY, headZ};

        // Calculate distance from local player
        float distance = Distance3D(localPos, feetPos);

        // Check if on screen (for debug) - NEW: Uses OpenGL's live matrices
        float screenFeet[2], screenHead[2];
        bool feetOnScreen = WorldToScreen(feetPos, screenFeet, transform);
        bool headOnScreen = WorldToScreen(headPos, screenHead, transform);

        // Skip if both feet and head are offscreen
        if (!feetOnScreen && !headOnScreen) {
            offScreenPlayers++;

            // Debug: Show why player is offscreen (only first 3 to avoid spam)
            if (playerDebugLine < 3) {
                char offscreenDbg[256];
                snprintf(offscreenDbg, sizeof(offscreenDbg), "  [OFFSCREEN] %s at (%.0f,%.0f,%.0f)", name, feetX, feetY, feetZ);
                drawList->AddText(ImVec2(10, debugY), IM_COL32(255, 100, 0, 255), offscreenDbg);
                debugY += debugLineHeight;
                playerDebugLine++;
            }
            continue;
        }

        // Debug: Show successful projection (only first 3)
        if (playerDebugLine < 3) {
            char projDbg[512];
            snprintf(projDbg, sizeof(projDbg), "  [VISIBLE] %s: Dist=%.1f", name, distance);
            drawList->AddText(ImVec2(10, debugY), IM_COL32(0, 255, 0, 255), projDbg);
            debugY += debugLineHeight;

            // Show world positions
            char worldDbg[256];
            snprintf(worldDbg, sizeof(worldDbg), "    World: Feet(%.1f,%.1f,%.1f) Head(%.1f,%.1f,%.1f)",
                     feetX, feetY, feetZ, headX, headY, headZ);
            drawList->AddText(ImVec2(10, debugY), IM_COL32(150, 150, 255, 255), worldDbg);
            debugY += debugLineHeight;

            // Show screen positions
            char screenDbg[256];
            snprintf(screenDbg, sizeof(screenDbg), "    Screen: Feet(%.0f,%.0f) Head(%.0f,%.0f)",
                     screenFeet[0], screenFeet[1], screenHead[0], screenHead[1]);
            drawList->AddText(ImVec2(10, debugY), IM_COL32(150, 255, 150, 255), screenDbg);
            debugY += debugLineHeight;

            playerDebugLine++;
        }

        // Choose color based on distance (red = close, yellow = far)
        float r = 1.0f, g = 0.0f, b = 0.0f;
        if (distance > 50.0f) {
            r = 1.0f; g = 1.0f; b = 0.0f;  // Yellow for far enemies
        }

        // ===== DRAW ESP ELEMENTS USING OPENGL (NEW: Uses live matrices!) =====

        // Option 1: Draw 2D box (fast and simple)
        Draw2DBox(feetPos, headPos, r, g, b, transform);

        // Option 2: Draw 3D box (more detailed, uncomment to use)
        // Draw3DBox(feetPos, headPos, r, g, b, transform);

        // Option 3: Draw snapline from screen center to enemy
        DrawSnapline(feetPos, r, g, b, transform);

        // Draw player name above box using ImGui (for now - could be replaced with OpenGL text rendering)
        if (name[0] != 0 && headOnScreen) {
            ImVec2 textSize = ImGui::CalcTextSize(name);
            ImVec2 namePos(screenHead[0] - textSize.x/2, screenHead[1] - 15.0f);

            // Draw text background
            drawList->AddRectFilled(
                ImVec2(namePos.x - 2, namePos.y - 2),
                ImVec2(namePos.x + textSize.x + 2, namePos.y + textSize.y + 2),
                IM_COL32(0, 0, 0, 150)
            );
            drawList->AddText(namePos, IM_COL32(255, 255, 255, 255), name);
        }
    }

    // ===== RESTORE OPENGL STATE =====
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Summary stats at bottom of debug panel
    char summaryText[128];
    snprintf(summaryText, sizeof(summaryText), "Valid: %d | OffScreen: %d | Rendered: %d",
             validPlayers, offScreenPlayers, validPlayers - offScreenPlayers);
    drawList->AddText(ImVec2(10, debugY), IM_COL32(255, 255, 0, 255), summaryText);
}
