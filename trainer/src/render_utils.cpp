#include "pch.h"
#include "render_utils.h"

#include <algorithm>
#include <cmath>

#include <gl/GL.h>

namespace {

// Manual implementation of gluProject (to avoid glu32.lib dependency)
bool ManualGLUProject(double objX, double objY, double objZ,
                      const double* model, const double* proj, const int* viewport,
                      double* winX, double* winY, double* winZ) {
    double in[4] = {objX, objY, objZ, 1.0};
    double out[4];

    // Multiply by modelview matrix (column-major layout from OpenGL)
    out[0] = model[0] * in[0] + model[4] * in[1] + model[8] * in[2] + model[12] * in[3];
    out[1] = model[1] * in[0] + model[5] * in[1] + model[9] * in[2] + model[13] * in[3];
    out[2] = model[2] * in[0] + model[6] * in[1] + model[10] * in[2] + model[14] * in[3];
    out[3] = model[3] * in[0] + model[7] * in[1] + model[11] * in[2] + model[15] * in[3];

    // Multiply by projection matrix (column-major layout)
    in[0] = proj[0] * out[0] + proj[4] * out[1] + proj[8] * out[2] + proj[12] * out[3];
    in[1] = proj[1] * out[0] + proj[5] * out[1] + proj[9] * out[2] + proj[13] * out[3];
    in[2] = proj[2] * out[0] + proj[6] * out[1] + proj[10] * out[2] + proj[14] * out[3];
    in[3] = proj[3] * out[0] + proj[7] * out[1] + proj[11] * out[2] + proj[15] * out[3];

    if (in[3] < 0.0001) {
        return false;
    }

    in[0] /= in[3];
    in[1] /= in[3];
    in[2] /= in[3];

    *winX = viewport[0] + (1.0 + in[0]) * viewport[2] / 2.0;
    *winY = viewport[1] + (1.0 + in[1]) * viewport[3] / 2.0;
    *winZ = (1.0 + in[2]) / 2.0;

    return true;
}

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

}  // namespace

namespace RenderUtils {

bool WorldToScreen(const Vec3& world, float screen[2]) {
    GLdouble modelview[16];
    GLdouble projection[16];
    GLint viewport[4];

    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    GLdouble screenX, screenY, screenZ;
    bool result = ManualGLUProject(world.x, world.y, world.z,
                                   modelview, projection, viewport,
                                   &screenX, &screenY, &screenZ);

    if (!result || screenZ < 0.0 || screenZ > 1.0) {
        return false;
    }

    screen[0] = static_cast<float>(screenX);
    screen[1] = static_cast<float>(viewport[3] - screenY);

    return true;
}

void Draw2DBox(const Vec3& feet, const Vec3& head, float r, float g, float b) {
    float screenFeet[2];
    float screenHead[2];

    if (!WorldToScreen(feet, screenFeet)) {
        return;
    }
    if (!WorldToScreen(head, screenHead)) {
        return;
    }

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int screenWidth = viewport[2];
    int screenHeight = viewport[3];

    if (screenFeet[0] < -100 || screenFeet[0] > screenWidth + 100 ||
        screenFeet[1] < -100 || screenFeet[1] > screenHeight + 100 ||
        screenHead[0] < -100 || screenHead[0] > screenWidth + 100 ||
        screenHead[1] < -100 || screenHead[1] > screenHeight + 100) {
        return;
    }

    float height = (std::abs)(screenFeet[1] - screenHead[1]);

    if (height < 10.0f || height > screenHeight * 2.0f) {
        return;
    }

    float width = height * 0.4f;
    float topY = (std::min)(screenFeet[1], screenHead[1]);
    float centerX = (screenFeet[0] + screenHead[0]) * 0.5f;

    float x = centerX - width / 2.0f;
    float y = topY;

    DrawFilledBoxGL(x, y, width, height, 0.0f, 0.0f, 0.0f, 0.4f);
    DrawBoxGL(x, y, width, height, r, g, b, 1.0f);
}

void DrawSnapline(const Vec3& targetPos, float r, float g, float b) {
    float screenPos[2];
    if (!WorldToScreen(targetPos, screenPos)) {
        return;
    }

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    float centerX = viewport[2] / 2.0f;
    float centerY = viewport[3] / 2.0f;

    DrawLineGL(centerX, centerY, screenPos[0], screenPos[1], r, g, b, 0.7f);
}

void Draw3DBox(const Vec3& feet, const Vec3& head, float r, float g, float b) {
    float boxWidth = 0.4f;
    Vec3 corners[8];

    corners[0] = {feet.x - boxWidth, feet.y - boxWidth, feet.z};
    corners[1] = {feet.x + boxWidth, feet.y - boxWidth, feet.z};
    corners[2] = {feet.x + boxWidth, feet.y + boxWidth, feet.z};
    corners[3] = {feet.x - boxWidth, feet.y + boxWidth, feet.z};

    corners[4] = {head.x - boxWidth, head.y - boxWidth, head.z};
    corners[5] = {head.x + boxWidth, head.y - boxWidth, head.z};
    corners[6] = {head.x + boxWidth, head.y + boxWidth, head.z};
    corners[7] = {head.x - boxWidth, head.y + boxWidth, head.z};

    float screenCorners[8][2];
    for (int i = 0; i < 8; ++i) {
        if (!WorldToScreen(corners[i], screenCorners[i])) {
            return;
        }
    }

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, 1.0f);
    glLineWidth(2.0f);

    glBegin(GL_LINES);
    for (int i = 0; i < 4; ++i) {
        glVertex2f(screenCorners[i][0], screenCorners[i][1]);
        glVertex2f(screenCorners[(i + 1) % 4][0], screenCorners[(i + 1) % 4][1]);
    }
    for (int i = 4; i < 8; ++i) {
        glVertex2f(screenCorners[i][0], screenCorners[i][1]);
        glVertex2f(screenCorners[i == 7 ? 4 : i + 1][0], screenCorners[i == 7 ? 4 : i + 1][1]);
    }
    for (int i = 0; i < 4; ++i) {
        glVertex2f(screenCorners[i][0], screenCorners[i][1]);
        glVertex2f(screenCorners[i + 4][0], screenCorners[i + 4][1]);
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

float Distance3D(const Vec3& a, const Vec3& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace RenderUtils

