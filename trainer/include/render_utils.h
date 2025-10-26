#pragma once

namespace RenderUtils {

struct Vec3 {
    float x;
    float y;
    float z;
};

struct TransformContext {
    double modelview[16];
    double projection[16];
    int viewport[4];
};

// Project a world coordinate into screen space using the provided matrices.
bool WorldToScreen(const Vec3& world, float screen[2], const TransformContext& context);

// Draw helpers for ESP overlays.
void Draw2DBox(const Vec3& feet, const Vec3& head, float r, float g, float b, const TransformContext& context);
void DrawSnapline(const Vec3& targetPos, float r, float g, float b, const TransformContext& context);
void Draw3DBox(const Vec3& feet, const Vec3& head, float r, float g, float b, const TransformContext& context);

// Basic vector utility.
float Distance3D(const Vec3& a, const Vec3& b);

}  // namespace RenderUtils
