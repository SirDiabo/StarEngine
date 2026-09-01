#pragma once
#include "raylib.h"

class StarMap;

namespace GameSession {

void BeginSession(StarMap& map);
void EndSession();
void UpdateActiveCamera(Camera3D& camera);

}
