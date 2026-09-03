#include "Nodes_Widgets.h"
#include "NodeRegistry.h"
#include "ObjectPinRef.h"
#include "ExecutionContext.h"
#include "Stargraph.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/Widget.h"
#include "Runtime/Components/WidgetComponent.h"
#include "Runtime/Actor.h"

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

    NodeTemplate getParentActor;
    getParentActor.type = "Get_Parent_Actor_From_Widget";
    getParentActor.category = "Widgets";
    getParentActor.moduleName = "Stargraph";
    getParentActor.hideFromContextMenu = true;
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
    elementEvent.createPins = [](Node* node) {
        node->inputs.push_back({"Element", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Clicked", PinType::Exec, false, true});
        node->outputs.push_back({"Selected", PinType::Exec, false, true});
        node->outputs.push_back({"Deselected", PinType::Exec, false, true});
    };
    elementEvent.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) { };
    NodeRegistry::Get().RegisterNode(elementEvent);
}
