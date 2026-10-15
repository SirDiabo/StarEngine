#include "ObjectEvents.h"
#include "NodeRegistry.h"
#include "ObjectPinRef.h"
#include "Runtime/Actor.h"
#include "Runtime/GraphBehavior.h"
#include "Runtime/Components/Component.h"
#include <map>

namespace {

std::map<std::string, std::vector<std::string>>& WidgetEventTable() {
    static std::map<std::string, std::vector<std::string>> table;
    return table;
}

std::string Sanitize(const std::string& text) {
    std::string result = text;
    for (char& c : result) if (c == ' ') c = '_';
    return result;
}

std::string StripSuffix(const std::string& text, const std::string& suffix) {
    if (text.size() > suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return text.substr(0, text.size() - suffix.size());
    }
    return text;
}

}

namespace ObjectEvents {

std::string ComponentEventNodeType(const std::string& componentType, const std::string& eventName) {
    return "Component_Event_" + Sanitize(componentType) + "_" + Sanitize(eventName);
}

std::string WidgetElementEventNodeType(const std::string& elementClass) {
    return "Widget_Element_Event_" + Sanitize(elementClass);
}

const std::vector<std::string>* GetWidgetElementEvents(const std::string& elementClass) {
    auto it = WidgetEventTable().find(elementClass);
    return it != WidgetEventTable().end() ? &it->second : nullptr;
}

void RegisterComponentEvent(const std::string& componentType, const ObjectEventDecl& decl) {
    NodeTemplate nodeTemplate;
    nodeTemplate.type = ComponentEventNodeType(componentType, decl.name);
    nodeTemplate.category = StripSuffix(componentType, "Component") + " Events";
    nodeTemplate.moduleName = "Stargraph";
    nodeTemplate.isEvent = true;
    nodeTemplate.menuLabel = decl.name;
    nodeTemplate.boundComponentType = componentType;
    nodeTemplate.boundComponentEvent = decl.name;
    nodeTemplate.createPins = [componentType, params = decl.params](Node* node) {
        node->inputs.push_back({"Component", PinType::Object, true, false, 0, "", false, componentType});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        for (const ObjectEventParam& param : params) {
            node->outputs.push_back({param.name, param.type, false, true});
        }
    };
    NodeRegistry::Get().RegisterNode(nodeTemplate);
}

void RegisterWidgetElementEvents(const std::string& elementClass, const std::vector<std::string>& eventNames) {
    WidgetEventTable()[elementClass] = eventNames;

    NodeTemplate nodeTemplate;
    nodeTemplate.type = WidgetElementEventNodeType(elementClass);
    nodeTemplate.category = "Widget Events";
    nodeTemplate.moduleName = "Stargraph";
    nodeTemplate.isEvent = true;
    nodeTemplate.hideFromContextMenu = true;
    nodeTemplate.boundElementClass = elementClass;
    nodeTemplate.createPins = [eventNames](Node* node) {
        node->inputs.push_back({"Element", PinType::String, true, false, 0, ""});
        for (const std::string& eventName : eventNames) {
            node->outputs.push_back({eventName, PinType::Exec, false, true});
        }
    };
    NodeRegistry::Get().RegisterNode(nodeTemplate);
}

void RaiseComponentEvent(Component* component, const std::string& eventName,
                         const std::vector<std::pair<std::string, std::string>>& params) {
    if (!component) return;
    Actor* owner = component->GetOwningActor();
    if (!owner) return;
    GraphBehavior* behavior = owner->GetGraphBehavior();
    if (!behavior) return;
    behavior->OnComponentEvent(owner, component, eventName, params);
}

bool NodeTargetsComponent(Node& node, Actor* owner, Component* component,
                          const std::string& componentType, ExecutionContext& ctx) {
    std::string componentRef;
    for (const Pin& pin : node.inputs) {
        if (pin.name == "Component") { componentRef = pin.defaultValue; break; }
    }

    if (componentRef.empty()) {
        const std::vector<Component*>& all = owner->GetComponents();
        for (Component* candidate : all) {
            if (componentType == candidate->GetClassName()) return candidate == component;
        }
        return false;
    }

    Component* target = ObjectPinRef::ResolveAs<Component>(node, componentRef, componentType, ctx);
    if (!target) target = owner->FindComponentByName(componentRef);
    return target == component;
}

}
