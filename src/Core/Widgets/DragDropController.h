#pragma once
#include "raylib.h"
#include <string>

namespace DragDrop {

void Begin(const std::string& kind, const std::string& payload, const std::string& label, Vector2 pressPos);
void Update(Vector2 mousePos);

bool IsCandidate();
bool IsDragging();
bool Matches(const std::string& kind);
const std::string& Payload();
const std::string& Label();
Vector2 PressPos();

void Consume();
void Cancel();

}
