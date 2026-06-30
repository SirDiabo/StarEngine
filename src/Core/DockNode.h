#pragma once
#include "raylib.h"
#include <vector>
#include <string>
#include <memory>

struct DockNode {
    enum class Type { Leaf, SplitH, SplitV };

    Type type = Type::Leaf;

    std::unique_ptr<DockNode> first;
    std::unique_ptr<DockNode> second;
    float splitRatio = 0.5f;

    // Leaf panel names and which one is active.
    std::vector<std::string> panelNames;
    int activeTab = 0;

    Rectangle bounds = { 0, 0, 0, 0 };

    bool IsLeaf() const { return type == Type::Leaf; }
};