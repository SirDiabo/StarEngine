#pragma once
#include "Modules/Stargraph/ComponentRegistry.h"
#include "Core/CollisionLayers.h"
#include <functional>

namespace CollisionFilterFields {

template <class T>
ComponentFieldDesc Layer(std::function<bool(const ComponentDecl&)> visibleIf = {}) {
    ComponentFieldDesc d = ComponentField::Enm("CollisionLayer", "Collision Layer", {}, "Default",
        [](Component& c, const ComponentDecl& decl) {
            int index = CollisionLayers::IndexOf(decl.Get("CollisionLayer", "Default"));
            static_cast<T&>(c).collisionLayer = index < 0 ? 0 : index;
        },
        [](const Component& c) -> std::string {
            const std::string& name = CollisionLayers::NameAt(static_cast<const T&>(c).collisionLayer);
            return name.empty() ? std::string("Default") : name;
        });
    d.enumProvider = []() { return CollisionLayers::Names(); };
    d.visibleIf = std::move(visibleIf);
    return d;
}

template <class T>
ComponentFieldDesc Mask(std::function<bool(const ComponentDecl&)> visibleIf = {}) {
    ComponentFieldDesc d;
    d.key = "CollisionMask";
    d.label = "Collides With";
    d.kind = ComponentFieldKind::LayerMask;
    d.defaultValue = CollisionLayers::MaskToString(CollisionLayers::kAllMask);
    d.apply = [](Component& c, const ComponentDecl& decl) {
        static_cast<T&>(c).collisionMask = CollisionLayers::ParseMask(decl.Get("CollisionMask", "4294967295"));
    };
    d.read = [](const Component& c) {
        return CollisionLayers::MaskToString(static_cast<const T&>(c).collisionMask);
    };
    d.visibleIf = std::move(visibleIf);
    return d;
}

}
