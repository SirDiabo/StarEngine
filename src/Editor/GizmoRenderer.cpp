#include "Core/RotationMath.h"
#include "GizmoRenderer.h"
#include "UITheme.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

static void BuildOrthonormalBasis(Vector3 n, Vector3& u, Vector3& v) {
    Vector3 helper = (fabsf(n.y) < 0.99f) ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
    u = Vector3Normalize(Vector3CrossProduct(helper, n));
    v = Vector3CrossProduct(n, u);
}

static float DistancePointToSegment2D(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ab = {b.x - a.x, b.y - a.y};
    float len2 = ab.x * ab.x + ab.y * ab.y;
    float t = 0.0f;
    if (len2 > 0.0001f) {
        t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    Vector2 proj = {a.x + ab.x * t, a.y + ab.y * t};
    return Vector2Distance(p, proj);
}

static Color BrightenColor(Color c) {
    auto lift = [](unsigned char v) { return (unsigned char)std::min(255, (int)v + 70); };
    return {lift(c.r), lift(c.g), lift(c.b), 255};
}

namespace GizmoRenderer {

Vector2 WorldToScreenPoint(Camera3D camera, int fboW, int fboH, Vector3 worldPos) {
    Vector2 fboSize = {(float)fboW, (float)fboH};
    Matrix proj = MatrixPerspective(camera.fovy * DEG2RAD, fboSize.x / fboSize.y, 1.0f, 50000.0f);
    Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    Matrix mvp  = MatrixMultiply(view, proj);

    Vector4 clip = {
        worldPos.x * mvp.m0 + worldPos.y * mvp.m4 + worldPos.z * mvp.m8  + mvp.m12,
        worldPos.x * mvp.m1 + worldPos.y * mvp.m5 + worldPos.z * mvp.m9  + mvp.m13,
        worldPos.x * mvp.m2 + worldPos.y * mvp.m6 + worldPos.z * mvp.m10 + mvp.m14,
        worldPos.x * mvp.m3 + worldPos.y * mvp.m7 + worldPos.z * mvp.m11 + mvp.m15
    };

    if (clip.w <= 0.0001f) return {-99999.0f, -99999.0f};

    Vector2 ndc = {clip.x / clip.w, clip.y / clip.w};
    return { (ndc.x + 1.0f) * 0.5f * fboSize.x, (1.0f - ndc.y) * 0.5f * fboSize.y };
}

float GetWorldPerPixel(Camera3D camera, int fboH, Vector3 pos) {
    float dist = Vector3Distance(camera.position, pos);
    return dist * tanf(camera.fovy * 0.5f * DEG2RAD) / ((float)fboH * 0.5f);
}

float GetGizmoWorldScale(Camera3D camera, int fboH, Vector3 pos) {
    const float desiredPixels = 90.0f;
    return desiredPixels * GetWorldPerPixel(camera, fboH, pos);
}

void GetGizmoAxes(GizmoSpace space, Vector3 rot, Vector3& axisX, Vector3& axisY, Vector3& axisZ) {
    if (space == GizmoSpace::World) {
        axisX = {1, 0, 0};
        axisY = {0, 1, 0};
        axisZ = {0, 0, 1};
        return;
    }

    Matrix rotMat = ComposeRotationDegrees(rot);
    axisX = Vector3Transform({1, 0, 0}, rotMat);
    axisY = Vector3Transform({0, 1, 0}, rotMat);
    axisZ = Vector3Transform({0, 0, 1}, rotMat);
}

void Render(Camera3D camera, int fboW, int fboH, GizmoMode mode, GizmoSpace space,
            Vector3 pos, Vector3 rot, const GizmoDragState& state) {
    if (mode == GizmoMode::Select) return;

    Vector3 axisX, axisY, axisZ;
    GetGizmoAxes(space, rot, axisX, axisY, axisZ);
    Vector3 axes[3] = {axisX, axisY, axisZ};
    Color colors[3] = { {230, 60, 60, 255}, {70, 210, 90, 255}, {70, 130, 240, 255} };
    float len = GetGizmoWorldScale(camera, fboH, pos);
    float worldPerPixel = GetWorldPerPixel(camera, fboH, pos);

    rlDrawRenderBatchActive();
    rlDisableDepthTest();

    if (mode == GizmoMode::Move) {
        for (int a = 0; a < 3; a++) {
            bool active = (state.hoverAxis == a || state.dragAxis == a);
            float shaftLen = len * 0.75f;
            float tipLen = len * 0.25f;
            float shaftRadius = (active ? 4.0f : 2.5f) * worldPerPixel;
            float tipRadius = (active ? 12.0f : 8.0f) * worldPerPixel;
            Color c = active ? BrightenColor(colors[a]) : colors[a];

            Vector3 shaftEnd = Vector3Add(pos, Vector3Scale(axes[a], shaftLen));
            Vector3 tipEnd = Vector3Add(pos, Vector3Scale(axes[a], shaftLen + tipLen));

            DrawCylinderEx(pos, shaftEnd, shaftRadius, shaftRadius, 8, c);
            DrawCylinderEx(shaftEnd, tipEnd, tipRadius, 0.0f, 12, c);
        }
    } else if (mode == GizmoMode::Scale) {
        for (int a = 0; a < 3; a++) {
            bool active = (state.hoverAxis == a || state.dragAxis == a);
            float shaftLen = len * 0.8f;
            float shaftRadius = (active ? 4.0f : 2.5f) * worldPerPixel;
            float cubeSize = (active ? 16.0f : 11.0f) * worldPerPixel;
            Color c = active ? BrightenColor(colors[a]) : colors[a];

            Vector3 shaftEnd = Vector3Add(pos, Vector3Scale(axes[a], shaftLen));
            DrawCylinderEx(pos, shaftEnd, shaftRadius, shaftRadius, 8, c);
            DrawCube(shaftEnd, cubeSize, cubeSize, cubeSize, c);
        }
    } else if (mode == GizmoMode::Rotate) {
        const int segments = 32;
        for (int a = 0; a < 3; a++) {
            bool active = (state.hoverAxis == a || state.dragAxis == a);
            Color c = active ? BrightenColor(colors[a]) : colors[a];

            Vector3 u, v;
            BuildOrthonormalBasis(axes[a], u, v);
            Vector3 prevPoint = Vector3Add(pos, Vector3Scale(u, len));
            for (int s = 1; s <= segments; s++) {
                float t = (float)s / (float)segments * 2.0f * PI;
                Vector3 pt = Vector3Add(pos, Vector3Add(Vector3Scale(u, cosf(t) * len), Vector3Scale(v, sinf(t) * len)));
                DrawLine3D(prevPoint, pt, c);
                prevPoint = pt;
            }
        }
    }

    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

int PickHandle(Camera3D camera, int fboW, int fboH, GizmoMode mode, GizmoSpace space,
               Vector2 localMp, Vector3 pos, Vector3 rot) {
    if (mode == GizmoMode::Select) return -1;

    Vector3 axisX, axisY, axisZ;
    GetGizmoAxes(space, rot, axisX, axisY, axisZ);
    Vector3 axes[3] = {axisX, axisY, axisZ};
    float len = GetGizmoWorldScale(camera, fboH, pos);

    float bestDist = 12.0f;
    int bestAxis = -1;

    if (mode == GizmoMode::Rotate) {
        const int segments = 32;
        for (int a = 0; a < 3; a++) {
            Vector3 u, v;
            BuildOrthonormalBasis(axes[a], u, v);
            Vector2 prevScreen = WorldToScreenPoint(camera, fboW, fboH, Vector3Add(pos, Vector3Scale(u, len)));
            for (int s = 1; s <= segments; s++) {
                float t = (float)s / (float)segments * 2.0f * PI;
                Vector3 pt = Vector3Add(pos, Vector3Add(Vector3Scale(u, cosf(t) * len), Vector3Scale(v, sinf(t) * len)));
                Vector2 ptScreen = WorldToScreenPoint(camera, fboW, fboH, pt);
                float d = DistancePointToSegment2D(localMp, prevScreen, ptScreen);
                if (d < bestDist) { bestDist = d; bestAxis = a; }
                prevScreen = ptScreen;
            }
        }
    } else {
        Vector2 originScreen = WorldToScreenPoint(camera, fboW, fboH, pos);
        for (int a = 0; a < 3; a++) {
            Vector2 tipScreen = WorldToScreenPoint(camera, fboW, fboH, Vector3Add(pos, Vector3Scale(axes[a], len)));
            float d = DistancePointToSegment2D(localMp, originScreen, tipScreen);
            if (d < bestDist) { bestDist = d; bestAxis = a; }
        }
    }

    return bestAxis;
}

bool UpdateDrag(Camera3D camera, int fboW, int fboH, GizmoMode mode, GizmoSpace space,
                 Rectangle client, bool snapEnabled, float gridStep, bool selectionValid,
                 Vector3& pos, Vector3& rot, Vector3& scale, GizmoDragState& state, bool& changed) {
    changed = false;

    if (mode == GizmoMode::Select || !selectionValid) {
        state.hoverAxis = -1;
        state.dragAxis = -1;
        return false;
    }

    Vector2 mp = GetMousePosition();
    Vector2 localMp = {mp.x - client.x, mp.y - client.y};

    if (state.dragAxis == -1) {
        state.hoverAxis = CheckCollisionPointRec(mp, client) ? PickHandle(camera, fboW, fboH, mode, space, localMp, pos, rot) : -1;

        if (state.hoverAxis != -1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            state.dragAxis = state.hoverAxis;
            state.dragStartMouse = localMp;
            state.dragStartPos = pos;
            state.dragStartRot = rot;
            state.dragStartScale = scale;
            state.dragHandleLen = GetGizmoWorldScale(camera, fboH, pos);
            state.dragWorldPerPixel = GetWorldPerPixel(camera, fboH, pos);

            Vector3 axisX, axisY, axisZ;
            GetGizmoAxes(space, rot, axisX, axisY, axisZ);
            Vector3 axes[3] = {axisX, axisY, axisZ};
            Vector2 originScreen = WorldToScreenPoint(camera, fboW, fboH, pos);
            Vector2 tipScreen = WorldToScreenPoint(camera, fboW, fboH, Vector3Add(pos, Vector3Scale(axes[state.dragAxis], state.dragHandleLen)));
            Vector2 dir2D = {tipScreen.x - originScreen.x, tipScreen.y - originScreen.y};
            float mag = sqrtf(dir2D.x * dir2D.x + dir2D.y * dir2D.y);
            state.dragAxisDir2D = (mag > 0.001f) ? Vector2{dir2D.x / mag, dir2D.y / mag} : Vector2{1, 0};

            if (mode == GizmoMode::Rotate) {
                state.dragStartScreenAngle = atan2f(localMp.y - originScreen.y, localMp.x - originScreen.x);
                Vector3 toCamera = Vector3Normalize(Vector3Subtract(camera.position, pos));
                state.dragRotSign = (Vector3DotProduct(toCamera, axes[state.dragAxis]) >= 0.0f) ? 1.0f : -1.0f;
            }
            return true;
        }
        return false;
    }

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        state.dragAxis = -1;
        return false;
    }

    if (mode == GizmoMode::Move) {
        Vector2 delta = {localMp.x - state.dragStartMouse.x, localMp.y - state.dragStartMouse.y};
        float t = delta.x * state.dragAxisDir2D.x + delta.y * state.dragAxisDir2D.y;
        float worldDelta = t * state.dragWorldPerPixel;

        Vector3 axisX, axisY, axisZ;
        GetGizmoAxes(space, state.dragStartRot, axisX, axisY, axisZ);
        Vector3 axes[3] = {axisX, axisY, axisZ};
        Vector3 newPos = Vector3Add(state.dragStartPos, Vector3Scale(axes[state.dragAxis], worldDelta));
        if (snapEnabled || IsKeyDown(KEY_LEFT_CONTROL)) {
            float step = (gridStep > 0.001f) ? gridStep : 1.0f;
            newPos.x = roundf(newPos.x / step) * step;
            newPos.y = roundf(newPos.y / step) * step;
            newPos.z = roundf(newPos.z / step) * step;
        }
        pos = newPos;
    } else if (mode == GizmoMode::Scale) {
        Vector2 delta = {localMp.x - state.dragStartMouse.x, localMp.y - state.dragStartMouse.y};
        float t = delta.x * state.dragAxisDir2D.x + delta.y * state.dragAxisDir2D.y;
        float worldDelta = t * state.dragWorldPerPixel;
        float scaleDelta = worldDelta / state.dragHandleLen;

        float newScale[3] = {state.dragStartScale.x, state.dragStartScale.y, state.dragStartScale.z};
        newScale[state.dragAxis] += scaleDelta;
        if (IsKeyDown(KEY_LEFT_CONTROL)) newScale[state.dragAxis] = roundf(newScale[state.dragAxis] / 0.1f) * 0.1f;
        if (newScale[state.dragAxis] < 0.01f) newScale[state.dragAxis] = 0.01f;
        scale = {newScale[0], newScale[1], newScale[2]};
    } else if (mode == GizmoMode::Rotate) {
        Vector2 originScreen = WorldToScreenPoint(camera, fboW, fboH, state.dragStartPos);
        float currentAngle = atan2f(localMp.y - originScreen.y, localMp.x - originScreen.x);
        float deltaDeg = state.dragRotSign * (state.dragStartScreenAngle - currentAngle) * RAD2DEG;
        if (IsKeyDown(KEY_LEFT_CONTROL)) deltaDeg = roundf(deltaDeg);

        Vector3 axisX, axisY, axisZ;
        GetGizmoAxes(space, state.dragStartRot, axisX, axisY, axisZ);
        Vector3 axes[3] = {axisX, axisY, axisZ};

        Quaternion startQuat = QuaternionFromEuler(state.dragStartRot.x * DEG2RAD, state.dragStartRot.y * DEG2RAD, state.dragStartRot.z * DEG2RAD);
        Quaternion deltaQuat = QuaternionFromAxisAngle(axes[state.dragAxis], deltaDeg * DEG2RAD);
        Quaternion newQuat = QuaternionMultiply(deltaQuat, startQuat);
        Vector3 newRotRad = QuaternionToEuler(newQuat);

        rot = {newRotRad.x * RAD2DEG, newRotRad.y * RAD2DEG, newRotRad.z * RAD2DEG};
    }

    changed = true;
    return true;
}

bool ToggleButton(Rectangle r, const std::string& iconFile, bool active, const GizmoIconLoader& getIcon) {
    const UIPalette& theme = UITheme::Get();
    Vector2 mp = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mp, r);

    Color bg = active ? theme.buttonPressed : (hovered ? theme.buttonHover : theme.buttonBg);
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 1, active ? theme.success : theme.buttonBorder);

    Texture2D tex = getIcon(iconFile);
    if (tex.id != 0) {
        Rectangle dst = {r.x + 3, r.y + 3, r.width - 6, r.height - 6};
        DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height}, dst, {0, 0}, 0, WHITE);
    }

    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

}
