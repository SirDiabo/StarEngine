#include "Nodes_Widgets.h"
#include "NodeRegistry.h"
#include "ObjectPinRef.h"
#include "ObjectEvents.h"
#include "ExecutionContext.h"
#include "Stargraph.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/Widget.h"
#include "Runtime/Components/WidgetComponent.h"
#include "Runtime/Actor.h"

static WidgetElement* FindWidgetElementByName(WidgetElement* root, const std::string& name) {
    if (!root) return nullptr;
    if (root->elementId == name) return root;
    for (WidgetElement* child : root->GetChildren()) {
        if (WidgetElement* found = FindWidgetElementByName(child, name)) return found;
    }
    return nullptr;
}

void RegisterWidgetNodes() {
    NodeTemplate setText;
    setText.type = "Set_Text_Element_Text";
    setText.category = "Widgets";
    setText.moduleName = "Stargraph";
    setText.hideFromContextMenu = true;
    setText.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "TextElement"});
        node->inputs.push_back({"Text", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setText.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        WidgetElement* target = nullptr;
        std::string text;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<WidgetElement>(node, p.defaultValue, "TextElement", ctx);
            }
            if (p.name == "Text") text = p.defaultValue;
        }
        if (!target) return;
        if (TextElement* te = dynamic_cast<TextElement*>(target)) {
            te->text = text;
        }
    };
    NodeRegistry::Get().RegisterNode(setText);

    NodeTemplate getText;
    getText.type = "Get_Text_Element_Text";
    getText.category = "Widgets";
    getText.moduleName = "Stargraph";
    getText.hideFromContextMenu = true;
    getText.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "TextElement"});
        node->outputs.push_back({"Text", PinType::String, false, true});
    };
    getText.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        WidgetElement* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<WidgetElement>(node, p.defaultValue, "TextElement", ctx);
            }
        }
        if (!target) return;
        if (TextElement* te = dynamic_cast<TextElement*>(target)) {
            for (Pin& p : node.outputs) if (p.name == "Text") p.defaultValue = te->text;
        }
    };
    NodeRegistry::Get().RegisterNode(getText);

    NodeTemplate getElement;
    getElement.type = "Get_Widget_Element";
    getElement.category = "Widgets";
    getElement.moduleName = "Stargraph";
    getElement.hideFromContextMenu = true;
    getElement.createPins = [](Node* node) {
        node->inputs.push_back({"Element_Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Element", PinType::Object, false, true, 0, "", false, "WidgetElement"});
    };
    getElement.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string elementName;
        for (const Pin& p : node.inputs) if (p.name == "Element_Name") elementName = p.defaultValue;
        if (elementName.empty() || !ctx.widget || !ctx.widget->rootElement) return;
        WidgetElement* found = FindWidgetElementByName(ctx.widget->rootElement, elementName);
        if (!found) return;
        for (Pin& p : node.outputs) {
            if (p.name == "Element") p.defaultValue = ObjectPinRef::Encode(found->GetObjectKind(), found->GetObjectId());
        }
    };
    NodeRegistry::Get().RegisterNode(getElement);

    NodeTemplate getParentActor;
    getParentActor.type = "Get_Parent_Actor_From_Widget";
    getParentActor.category = "Widgets";
    getParentActor.moduleName = "Stargraph";
    getParentActor.menuLabel = "Get Parent Actor";
    getParentActor.createPins = [](Node* node) {
        node->outputs.push_back({"Actor", PinType::Object, false, true, 0, "", false, "Actor"});
    };
    getParentActor.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.widget || !ctx.widget->owningComponent) return;
        Actor* owner = ctx.widget->owningComponent->GetOwningActor();
        if (!owner) return;
        for (Pin& p : node.outputs) {
            if (p.name == "Actor") p.defaultValue = ObjectPinRef::Encode("Actor", owner->GetObjectId());
        }
    };
    NodeRegistry::Get().RegisterNode(getParentActor);

    NodeTemplate elementEvent;
    elementEvent.type = "Widget_Element_Event";
    elementEvent.category = "Widgets";
    elementEvent.moduleName = "Stargraph";
    elementEvent.isEvent = true;
    elementEvent.hideFromContextMenu = true;
    elementEvent.createPins = [](Node* node) {
        node->inputs.push_back({"Element", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Clicked", PinType::Exec, false, true});
        node->outputs.push_back({"Selected", PinType::Exec, false, true});
        node->outputs.push_back({"Deselected", PinType::Exec, false, true});
    };
    elementEvent.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) { };
    NodeRegistry::Get().RegisterNode(elementEvent);

    NodeTemplate widgetConstruct;
    widgetConstruct.type = "Widget_Event_Construct";
    widgetConstruct.category = "Widget Events";
    widgetConstruct.moduleName = "Stargraph";
    widgetConstruct.isEvent = true;
    widgetConstruct.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(widgetConstruct);

    NodeTemplate widgetTick;
    widgetTick.type = "Widget_Event_Tick";
    widgetTick.category = "Widget Events";
    widgetTick.moduleName = "Stargraph";
    widgetTick.isEvent = true;
    widgetTick.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Delta Seconds", PinType::Float, false, true});
    };
    NodeRegistry::Get().RegisterNode(widgetTick);

    NodeTemplate widgetDestruct;
    widgetDestruct.type = "Widget_Event_Destruct";
    widgetDestruct.category = "Widget Events";
    widgetDestruct.moduleName = "Stargraph";
    widgetDestruct.isEvent = true;
    widgetDestruct.createPins = [](Node* node) {
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    NodeRegistry::Get().RegisterNode(widgetDestruct);

    ObjectEvents::RegisterWidgetElementEvents("ButtonElement",
        { "Clicked", "Pressed", "Released", "Hovered", "Unhovered", "Selected", "Deselected" });

    NodeTemplate setWidget;
    setWidget.type = "Set_Widget";
    setWidget.category = "Widgets";
    setWidget.moduleName = "Stargraph";
    setWidget.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Component", PinType::Object, true, false, 0, "", false, "WidgetComponent"});
        node->inputs.push_back({"Widget Asset", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setWidget.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string componentRef;
        std::string assetPath;
        for (const Pin& p : node.inputs) {
            if (p.name == "Component") componentRef = p.defaultValue;
            else if (p.name == "Widget Asset") assetPath = p.defaultValue;
        }

        WidgetComponent* target = nullptr;
        if (!componentRef.empty()) {
            target = ObjectPinRef::ResolveAs<WidgetComponent>(node, componentRef, "WidgetComponent", ctx);
            if (!target && ctx.actor) target = dynamic_cast<WidgetComponent*>(ctx.actor->FindComponentByName(componentRef));
        }
        if (!target && ctx.actor) {
            const std::vector<Component*>& all = ctx.actor->GetComponents();
            for (auto it = all.rbegin(); it != all.rend() && !target; ++it) {
                target = dynamic_cast<WidgetComponent*>(*it);
            }
        }
        if (!target) return;
        target->widgetAssetPath = assetPath;
    };
    NodeRegistry::Get().RegisterNode(setWidget);
}
