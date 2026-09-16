#include "DragDropController.h"

namespace {

bool g_isCandidate = false;
bool g_isDragging = false;
std::string g_kind;
std::string g_payload;
std::string g_label;
Vector2 g_pressPos{};

}

namespace DragDrop {

void Begin(const std::string& kind, const std::string& payload, const std::string& label, Vector2 pressPos) {
    g_isCandidate = true;
    g_isDragging = false;
    g_kind = kind;
    g_payload = payload;
    g_label = label;
    g_pressPos = pressPos;
}

void Update(Vector2 mousePos) {
    if (!g_isCandidate) return;

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Cancel();
        return;
    }

    if (!g_isDragging) {
        float dx = mousePos.x - g_pressPos.x;
        float dy = mousePos.y - g_pressPos.y;
        if (dx * dx + dy * dy > 36.0f) g_isDragging = true;
    }

    if (g_isDragging) {
        int w = MeasureText(g_label.c_str(), 12) + 16;
        Rectangle ghost = { mousePos.x + 12, mousePos.y - 4, (float)w, 20 };
        DrawRectangleRec(ghost, {55, 55, 65, 220});
        DrawRectangleLinesEx(ghost, 1.0f, {120, 120, 140, 255});
        DrawText(g_label.c_str(), (int)ghost.x + 8, (int)ghost.y + 4, 12, WHITE);
    }
}

bool IsCandidate() {
    return g_isCandidate;
}

bool IsDragging() {
    return g_isDragging;
}

bool Matches(const std::string& kind) {
    return g_isDragging && g_kind == kind;
}

const std::string& Payload() {
    return g_payload;
}

const std::string& Label() {
    return g_label;
}

Vector2 PressPos() {
    return g_pressPos;
}

void Consume() {
    g_isCandidate = false;
    g_isDragging = false;
}

void Cancel() {
    g_isCandidate = false;
    g_isDragging = false;
}

}
