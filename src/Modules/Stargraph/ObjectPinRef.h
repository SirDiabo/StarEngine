#pragma once
#include "Stargraph.h"
#include <string>

class Object;
struct ExecutionContext;

namespace ObjectPinRef {
    std::string Encode(const std::string& kind, unsigned int objectId);

    Object* Resolve(const Node& node, const std::string& encodedValue,
                     const std::string& requiredKind, ExecutionContext& ctx);

    template<typename T>
    T* ResolveAs(const Node& node, const std::string& encodedValue,
                 const std::string& requiredKind, ExecutionContext& ctx) {
        return dynamic_cast<T*>(Resolve(node, encodedValue, requiredKind, ctx));
    }
}
