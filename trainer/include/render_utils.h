#pragma once

namespace RenderUtils {

struct Vec3 {
    float x;
    float y;
    float z;
};

// Project a world coordinate into screen space using the current OpenGL matrices.
bool WorldToScreen(const Vec3& world, float screen[2]);

// Draw helpers for ESP overlays.
void Draw2DBox(const Vec3& feet, const Vec3& head, float r, float g, float b);
void DrawSnapline(const Vec3& targetPos, float r, float g, float b);
void Draw3DBox(const Vec3& feet, const Vec3& head, float r, float g, float b);

// Basic vector utility.
float Distance3D(const Vec3& a, const Vec3& b);

}  // namespace RenderUtils

