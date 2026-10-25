#pragma once
#include "Runtime/ComponentDecl.h"
#include "raylib.h"
#include <string>
#include <vector>

struct BasicActorDef {
    std::string label;
    Color accentColor;
    std::string iconFile;
    std::string kindName;

    bool isMeshActor = false;

    std::string componentType;
    ComponentDecl defaultComponent;
};

const std::vector<BasicActorDef>& GetBasicActorDefs();
