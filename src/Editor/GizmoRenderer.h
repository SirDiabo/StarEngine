#pragma once
#include "raylib.h"
#include <string>
#include <functional>

enum class GizmoMode { Select, Move, Rotate, Scale };
enum class GizmoSpace { World, Local };

struct GizmoDragState {
    int hoverAxis = -1;
    int dragAxis = -1;
    Vector2 dragStartMouse{0, 0};
    Vector3 dragStartPos{0, 0, 0};
    Vector3 dragStartRot{0, 0, 0};
    Vector3 dragStartScale{0, 0, 0};
    Vector2 dragAxisDir2D{1, 0};
    float dragWorldPerPixel = 1.0f;
    float dragHandleLen = 1.0f;
    float dragStartScreenAngle = 0.0f;
    float dragRotSign = 1.0f;
};

using GizmoIconLoader = std::function<Texture2D(const std::string&)>;

namespace GizmoRenderer {

Vector2 WorldToScreenPoint(Camera3D camera, int fboW, int fboH, Vector3 worldPos);
float GetWorldPerPixel(Camera3D camera, int fboH, Vector3 pos);
float GetGizmoWorldScale(Camera3D camera, int fboH, Vector3 pos);
void GetGizmoAxes(GizmoSpace space, Vector3 rot, Vector3& axisX, Vector3& axisY, Vector3& axisZ);

void Render(Camera3D camera, int fboW, int fboH, GizmoMode mode, GizmoSpace space,
            Vector3 pos, Vector3 rot, const GizmoDragState& state);

int PickHandle(Camera3D camera, int fboW, int fboH, GizmoMode mode, GizmoSpace space,
               Vector2 localMp, Vector3 pos, Vector3 rot);

bool UpdateDrag(Camera3D camera, int fboW, int fboH, GizmoMode mode, GizmoSpace space,
                 Rectangle client, bool snapEnabled, float gridStep, bool selectionValid,
                 Vector3& pos, Vector3& rot, Vector3& scale, GizmoDragState& state, bool& changed);

bool ToggleButton(Rectangle r, const std::string& iconFile, bool active, const GizmoIconLoader& getIcon);

}
