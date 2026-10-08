#pragma once
#include "Actor.h"
#include "Components/Component.h"
#include <filesystem>
#include <string>

namespace ProfilerLabels {

inline std::string Stem(const std::string& path) {
    if (path.empty()) return std::string();
    return std::filesystem::path(path).stem().string();
}

inline std::string ForActor(const Actor& actor) {
    std::string label = actor.GetName();
    if (label.empty() || label == "Object") label = "Actor";
    std::string className = Stem(actor.GetActorClass());
    if (!className.empty() && className != label) label += " [" + className + "]";
    return label;
}

inline std::string ForComponent(const Component& component) {
    std::string label = component.GetName();
    std::string className = component.GetClassName();
    if (label.empty() || label == "Object") return className;
    if (label != className) label += " [" + className + "]";
    return label;
}

}
