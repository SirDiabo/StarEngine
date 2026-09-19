#pragma once
#include "PropertyPanel.h"
#include "Runtime/StarMap.h"
#include <functional>
#include <vector>

std::vector<PropertyRow> BuildActorPropertyRows(
    ActorInstance& inst,
    const std::function<ActorInstance*()>& currentInstance,
    const std::function<void()>& onEdited,
    const std::function<void(const std::string& path, const std::string& ext)>& onOpenAsset);
