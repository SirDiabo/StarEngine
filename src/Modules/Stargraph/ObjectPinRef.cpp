#include "ObjectPinRef.h"
#include "ExecutionContext.h"
#include "Core/ObjectRefRegistry.h"
#include "Core/ObjectKindRegistry.h"
#include "Runtime/Object.h"
#include "Runtime/Actor.h"
#include "Runtime/PrintMessageLog.h"

static std::string FormatNodeName(const Node& node) {
    std::string display = node.title.empty() ? node.type : node.title;
    for (char& c : display) if (c == '_') c = ' ';
    return display;
}

std::string ObjectPinRef::Encode(const std::string& kind, unsigned int objectId) {
    return kind + ":" + std::to_string(objectId);
}

Object* ObjectPinRef::Resolve(const Node& node, const std::string& encodedValue,
                               const std::string& requiredKind, ExecutionContext& ctx) {
    if (encodedValue.empty()) return nullptr;

    size_t sep = encodedValue.find(':');
    if (sep == std::string::npos) return nullptr;

    std::string kind = encodedValue.substr(0, sep);
    unsigned int id = 0;
    try {
        id = (unsigned int)std::stoul(encodedValue.substr(sep + 1));
    } catch (...) {
        return nullptr;
    }

    if (!ObjectKindRegistry::Get().IsCompatible(kind, requiredKind)) {
        std::string owner = ctx.actor ? ctx.actor->GetName() : "<none>";
        std::string text = "'" + FormatNodeName(node) + "' in '" + owner + "' received '" + kind +
                            "', expecting '" + requiredKind + "'";
        PrintMessageLog::Get().Add(PrintMessageType::Error, text, 6.0f);
        return nullptr;
    }

    return ObjectRefRegistry::Get().Resolve(id);
}
