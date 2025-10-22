#include "pch.h"
#include "ui.h"
#include "trainer.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include <cstdio>
#include <thread>
#include <algorithm>
#include <windowsx.h>

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
      selectedIndex(0),
      panelWidth(360),
      panelPadding(18),
      sectionSpacing(22),
      headerFont(nullptr),
      textFont(nullptr),
      smallFont(nullptr) {}

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

        ImVec2 minVec(start.x, y);
        ImVec2 maxVec(start.x + buttonWidth, y + kButtonHeight);

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

    if (!menuVisible) {
        selectedIndex = 0;
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

    UpdateMenuState();

    // ALWAYS draw a test rectangle to verify rendering works
    ImDrawList* testDrawList = ImGui::GetForegroundDrawList();
    testDrawList->AddRectFilled(ImVec2(10, 10), ImVec2(110, 60), IM_COL32(255, 0, 0, 255));
    testDrawList->AddText(ImVec2(15, 15), IM_COL32(255, 255, 255, 255), "HOOK ACTIVE");

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
    } else {
        featureToggles.clear();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool UIRenderer::ProcessInput(MSG& msg, bool& requestUnload) {
    requestUnload = false;

    if (!menuVisible) {
        // When menu is hidden, only consume raw input to avoid cursor issues
        if (msg.message == WM_INPUT) {
            return true;
        }
        return false;
    }

    bool handled = false;

    switch (msg.message) {
    case WM_INPUT:
        handled = true;
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        bool initialPress = ((msg.lParam & (1 << 30)) == 0);
        switch (msg.wParam) {
        case VK_UP:
            if (initialPress) {
                HandleKeyUp();
            }
            handled = true;
            break;
        case VK_DOWN:
            if (initialPress) {
                HandleKeyDown();
            }
            handled = true;
            break;
        case VK_RETURN:
        case VK_SPACE:
            if (initialPress) {
                if (HandleKeyEnter()) {
                    requestUnload = true;
                }
            }
            handled = true;
            break;
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
