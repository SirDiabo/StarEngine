#pragma once
#include "Modules/Stargraph/Stargraph.h"
#include "raylib.h"
#include <string>
#include <vector>

struct BasicActorDef {
    std::string label;
    Color accentColor;

    // True for the Mesh entry, which reuses the existing StaticMeshActor path
    // (className="StaticMeshActor", meshAssetPath left empty for the user to assign
    // via the Details panel) instead of the generic inline-component path below.
    bool isMeshActor = false;

    std::string componentType;
    ComponentDecl defaultComponent;
};

const std::vector<BasicActorDef>& GetBasicActorDefs();
