#pragma once
#include <vector>

namespace PointerEvents {

template<typename E>
struct Event {
    E* element;
    const char* name;
};

template<typename E, typename IsValid>
void Compute(E* previousHovered, E* previousPressOrigin, E* hovered, bool pressedThisFrame, bool leftDown,
             IsValid isValid, std::vector<Event<E>>& out) {
    if (hovered != previousHovered) {
        if (previousHovered && isValid(previousHovered)) out.push_back({ previousHovered, "Unhovered" });
        if (hovered) out.push_back({ hovered, "Hovered" });
    }

    E* pressTarget = pressedThisFrame ? hovered : nullptr;
    if (pressTarget) out.push_back({ pressTarget, "Pressed" });

    if (!leftDown) {
        E* releaseTarget = pressTarget ? pressTarget : previousPressOrigin;
        if (releaseTarget && (releaseTarget == pressTarget || isValid(releaseTarget))) {
            out.push_back({ releaseTarget, "Released" });
            if (hovered == releaseTarget) out.push_back({ releaseTarget, "Clicked" });
        }
    }
}

}
