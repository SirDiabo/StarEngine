#pragma once
#include "Stargraph.h"
#include <string>
#include <utility>
#include <vector>

class Actor;
class Component;
struct ExecutionContext;

struct ObjectEventParam {
    std::string name;
    PinType type = PinType::String;
    std::string objectKind;
};

struct ObjectEventDecl {
    std::string name;
    std::vector<ObjectEventParam> params;
};

namespace ObjectEvents {

void RegisterComponentEvent(const std::string& componentType, const ObjectEventDecl& decl);
void RegisterWidgetElementEvents(const std::string& elementClass, const std::vector<std::string>& eventNames);

std::string ComponentEventNodeType(const std::string& componentType, const std::string& eventName);
std::string WidgetElementEventNodeType(const std::string& elementClass);
const std::vector<std::string>* GetWidgetElementEvents(const std::string& elementClass);

void RaiseComponentEvent(Component* component, const std::string& eventName,
                         const std::vector<std::pair<std::string, std::string>>& params = {});

bool NodeTargetsComponent(Node& node, Actor* owner, Component* component,
                          const std::string& componentType, ExecutionContext& ctx);

}
