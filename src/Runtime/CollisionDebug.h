#pragma once
#include "raylib.h"

void DrawAndClearCollisionDebug(float viewportHeight = 0.0f);
void SetCollisionDebugEnabled(bool enabled);
bool IsCollisionDebugEnabled();
void AddCollisionDebugLine(Vector3 start, Vector3 end, Color color);
void AddCollisionDebugSphere(Vector3 center, float radius, Color color);
