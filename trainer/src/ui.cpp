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
#include <algorithm>
#include <windowsx.h>
#include <gl/GL.h>

// Simple vector structs for matrix math
struct Vec3 { float x, y, z; };

namespace {

// ============================================================================
// NEW APPROACH: Use OpenGL's ACTUAL live matrices via glGet*
// Manual gluProject implementation to avoid GLU dependency
// ============================================================================

// Manual implementation of gluProject (to avoid glu32.lib dependency)
// IMPORTANT: AssaultCube stores matrices in ROW-MAJOR format despite using OpenGL!
bool ManualGLUProject(double objX, double objY, double objZ,
                      const double* model, const double* proj, const int* viewport,
                      double* winX, double* winY, double* winZ) {
    // Transform object coordinates to eye coordinates using modelview matrix
    double in[4] = { objX, objY, objZ, 1.0 };
    double out[4];

    // Multiply by modelview matrix - ROW-MAJOR interpretation
    // Memory layout: [row0: m0 m1 m2 m3] [row1: m4 m5 m6 m7] [row2: m8 m9 m10 m11] [row3: m12 m13 m14 m15]
    out[0] = model[0]*in[0]  + model[1]*in[1]  + model[2]*in[2]   + model[3]*in[3];
    out[1] = model[4]*in[0]  + model[5]*in[1]  + model[6]*in[2]   + model[7]*in[3];
    out[2] = model[8]*in[0]  + model[9]*in[1]  + model[10]*in[2]  + model[11]*in[3];
    out[3] = model[12]*in[0] + model[13]*in[1] + model[14]*in[2]  + model[15]*in[3];

    // Multiply by projection matrix - ROW-MAJOR interpretation
    in[0] = proj[0]*out[0]  + proj[1]*out[1]  + proj[2]*out[2]   + proj[3]*out[3];
    in[1] = proj[4]*out[0]  + proj[5]*out[1]  + proj[6]*out[2]   + proj[7]*out[3];
    in[2] = proj[8]*out[0]  + proj[9]*out[1]  + proj[10]*out[2]  + proj[11]*out[3];
    in[3] = proj[12]*out[0] + proj[13]*out[1] + proj[14]*out[2]  + proj[15]*out[3];

    // Check if w is too small (behind camera)
    if (in[3] < 0.0001) return false;

    // Perspective divide
    in[0] /= in[3];
    in[1] /= in[3];
    in[2] /= in[3];

    // Map to viewport
    *winX = viewport[0] + (1.0 + in[0]) * viewport[2] / 2.0;
    *winY = viewport[1] + (1.0 + in[1]) * viewport[3] / 2.0;
    *winZ = (1.0 + in[2]) / 2.0;

    return true;
}

// World to screen using OpenGL's current matrices (CORRECT APPROACH)
bool WorldToScreen(const Vec3& world, float screen[2]) {
    // Get OpenGL's CURRENT matrices and viewport
    GLdouble modelview[16];
    GLdouble projection[16];
    GLint viewport[4];

    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Use our manual gluProject implementation
    GLdouble screenX, screenY, screenZ;
    bool result = ManualGLUProject(
        world.x, world.y, world.z,
        modelview, projection, viewport,
        &screenX, &screenY, &screenZ
    );

    // Check if projection succeeded and point is in front of camera
    if (!result || screenZ < 0.0 || screenZ > 1.0) {
        return false;
    }

    // Convert to screen coordinates (Y is bottom-up in OpenGL)
    screen[0] = static_cast<float>(screenX);
    screen[1] = static_cast<float>(viewport[3] - screenY);  // Flip Y for screen coords

    return true;
}

// ============================================================================
// OpenGL Drawing Helper Functions (from esp_implementation.cpp template)
// ============================================================================

// Draw a 2D line using OpenGL
void DrawLineGL(float x1, float y1, float x2, float y2, float r, float g, float b, float a) {
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, a);
    glLineWidth(2.0f);

    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

// Draw a 2D box using OpenGL
void DrawBoxGL(float x, float y, float width, float height, float r, float g, float b, float a) {
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, a);
    glLineWidth(2.0f);

    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

// Draw filled box using OpenGL
void DrawFilledBoxGL(float x, float y, float width, float height, float r, float g, float b, float a) {
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, a);

    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

// Draw a 2D ESP box (simple and fast) - NEW: Uses OpenGL's live matrices
void Draw2DBox(const Vec3& feet, const Vec3& head, float r, float g, float b) {
    float screenFeet[2], screenHead[2];

    // Project using OpenGL's CURRENT matrices
    if (!WorldToScreen(feet, screenFeet))
        return;
    if (!WorldToScreen(head, screenHead))
        return;

    // Get viewport for bounds checking
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int screenWidth = viewport[2];
    int screenHeight = viewport[3];

    // Validate screen coordinates are reasonable
    if (screenFeet[0] < -100 || screenFeet[0] > screenWidth + 100 ||
        screenFeet[1] < -100 || screenFeet[1] > screenHeight + 100 ||
        screenHead[0] < -100 || screenHead[0] > screenWidth + 100 ||
        screenHead[1] < -100 || screenHead[1] > screenHeight + 100) {
        return;  // Skip if way off screen
    }

    // Calculate box dimensions - use absolute value to handle any Y inversion
    float height = (std::abs)(screenFeet[1] - screenHead[1]);  // Parentheses avoid macro conflicts

    // Sanity check: height should be reasonable (not too small or huge)
    if (height < 10.0f || height > screenHeight * 2.0f) {
        return;  // Skip if box would be invalid
    }

    float width = height * 0.4f;  // Width is 40% of height

    // Use the minimum Y as top (head should be above feet)
    float topY = (std::min)(screenFeet[1], screenHead[1]);  // Parentheses avoid Windows.h min macro
    float centerX = (screenFeet[0] + screenHead[0]) * 0.5f;  // Average X position

    float x = centerX - width / 2.0f;
    float y = topY;

    // Draw filled background
    DrawFilledBoxGL(x, y, width, height, 0.0f, 0.0f, 0.0f, 0.4f);
    // Draw box outline
    DrawBoxGL(x, y, width, height, r, g, b, 1.0f);
}

// Draw snapline from screen center to target - NEW: Uses OpenGL's live matrices
void DrawSnapline(const Vec3& targetPos, float r, float g, float b) {
    float screenPos[2];
    if (!WorldToScreen(targetPos, screenPos))
        return;

    // Get viewport to find screen center
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    float centerX = viewport[2] / 2.0f;
    float centerY = viewport[3] / 2.0f;

    DrawLineGL(centerX, centerY, screenPos[0], screenPos[1], r, g, b, 0.7f);
}

// Draw a 3D box around a player - NEW: Uses OpenGL's live matrices
void Draw3DBox(const Vec3& feet, const Vec3& head, float r, float g, float b) {
    // Define 8 corners of the bounding box
    float boxWidth = 0.4f;  // Half-width of player
    Vec3 corners[8];

    // Bottom corners (feet level)
    corners[0] = {feet.x - boxWidth, feet.y - boxWidth, feet.z};
    corners[1] = {feet.x + boxWidth, feet.y - boxWidth, feet.z};
    corners[2] = {feet.x + boxWidth, feet.y + boxWidth, feet.z};
    corners[3] = {feet.x - boxWidth, feet.y + boxWidth, feet.z};

    // Top corners (head level)
    corners[4] = {head.x - boxWidth, head.y - boxWidth, head.z};
    corners[5] = {head.x + boxWidth, head.y - boxWidth, head.z};
    corners[6] = {head.x + boxWidth, head.y + boxWidth, head.z};
    corners[7] = {head.x - boxWidth, head.y + boxWidth, head.z};

    // Convert to screen coordinates using OpenGL's live matrices
    float screenCorners[8][2];
    for (int i = 0; i < 8; i++) {
        if (!WorldToScreen(corners[i], screenCorners[i]))
            return;  // Don't draw if any corner is behind camera
    }

    // Draw the box edges
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, 1.0f);
    glLineWidth(2.0f);

    glBegin(GL_LINES);
    // Bottom square
    for (int i = 0; i < 4; i++) {
        glVertex2f(screenCorners[i][0], screenCorners[i][1]);
        glVertex2f(screenCorners[(i + 1) % 4][0], screenCorners[(i + 1) % 4][1]);
    }
    // Top square
    for (int i = 4; i < 8; i++) {
        glVertex2f(screenCorners[i][0], screenCorners[i][1]);
        glVertex2f(screenCorners[i == 7 ? 4 : i + 1][0], screenCorners[i == 7 ? 4 : i + 1][1]);
    }
    // Vertical lines
    for (int i = 0; i < 4; i++) {
        glVertex2f(screenCorners[i][0], screenCorners[i][1]);
        glVertex2f(screenCorners[i + 4][0], screenCorners[i + 4][1]);
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

// Calculate distance between two points
float Distance3D(const Vec3& a, const Vec3& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
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
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Get screen dimensions
    RECT clientRect;
    if (!GetClientRect(gameWindow, &clientRect)) return;

    int screenWidth = clientRect.right;
    int screenHeight = clientRect.bottom;

    // ===== DEBUG PANEL - TOP LEFT (Still using ImGui for text) =====
    int debugY = 10;
    const int debugLineHeight = 16;

    // ESP Status
    drawList->AddRectFilled(ImVec2(5, debugY - 2), ImVec2(400, debugY + 130), IM_COL32(0, 0, 0, 180));
    drawList->AddText(ImVec2(10, debugY), IM_COL32(255, 0, 255, 255), "=== ESP DEBUG (Live GL Matrices) ===");
    debugY += debugLineHeight;

    // NEW: Read matrices directly from OpenGL for debug
    GLdouble modelview[16];
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);

    // Matrix debug - show key elements
    char matrixDebug[256];
    snprintf(matrixDebug, sizeof(matrixDebug), "ModelView: [0-3]=%.2f,%.2f,%.2f,%.2f [12-15]=%.2f,%.2f,%.2f,%.2f",
             (float)modelview[0], (float)modelview[1], (float)modelview[2], (float)modelview[3],
             (float)modelview[12], (float)modelview[13], (float)modelview[14], (float)modelview[15]);
    drawList->AddText(ImVec2(10, debugY), IM_COL32(200, 200, 255, 255), matrixDebug);
    debugY += debugLineHeight;

    // Show screen dimensions
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
        bool feetOnScreen = WorldToScreen(feetPos, screenFeet);
        bool headOnScreen = WorldToScreen(headPos, screenHead);

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
        Draw2DBox(feetPos, headPos, r, g, b);

        // Option 2: Draw 3D box (more detailed, uncomment to use)
        // Draw3DBox(feetPos, headPos, r, g, b);

        // Option 3: Draw snapline from screen center to enemy
        DrawSnapline(feetPos, r, g, b);

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
