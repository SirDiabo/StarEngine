#include "Nodes_Widgets.h"
#include "NodeRegistry.h"
#include "ObjectPinRef.h"
#include "ObjectEvents.h"
#include "ExecutionContext.h"
#include "Stargraph.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetObjectElement.h"
#include "Core/Widgets/ColorPickerElement.h"
#include "Core/Widgets/WidgetElementEvents.h"
#include "Runtime/WidgetBehavior.h"
#include <cstdio>
#include "StargraphBehavior.h"
#include "GraphExecutor.h"
#include "Runtime/Components/WidgetComponent.h"
#include "Runtime/Actor.h"

static WidgetElement* FindWidgetElementByName(WidgetElement* root, const std::string& name) {
    if (!root) return nullptr;
    if (root->elementId == name) return root;
    for (WidgetElement* child : root->GetAuthoredChildren()) {
        if (WidgetElement* found = FindWidgetElementByName(child, name)) return found;
    }
    return nullptr;
}

std::vector<std::pair<std::string, std::string>> ParseWidgetCallArgs(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> out;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(';', start);
        if (end == std::string::npos) end = text.size();
        std::string item = text.substr(start, end - start);
        size_t eq = item.find('=');
        if (eq != std::string::npos && eq > 0) out.emplace_back(item.substr(0, eq), item.substr(eq + 1));
        start = end + 1;
    }
    return out;
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


    NodeTemplate setWidget;
    setWidget.type = "Set_Widget";
    setWidget.category = "Widgets";
    setWidget.moduleName = "Stargraph";
    setWidget.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Component", PinType::Object, true, false, 0, "", false, "WidgetComponent"});
        node->inputs.push_back({"Widget Asset", PinType::String, true, false, 0, "", false, "Widget"});
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
            for (Component* candidate : all) {
                target = dynamic_cast<WidgetComponent*>(candidate);
                if (target) break;
            }
        }
        if (!target) return;
        target->widgetAssetPath = assetPath;
    };
    NodeRegistry::Get().RegisterNode(setWidget);

    NodeTemplate addWidgetObject;
    addWidgetObject.type = "Add_Widget_Object";
    addWidgetObject.category = "Widgets";
    addWidgetObject.moduleName = "Stargraph";
    addWidgetObject.menuLabel = "Add Widget Object";
    addWidgetObject.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Container", PinType::Object, true, false, 0, "", false, "WidgetElement"});
        node->inputs.push_back({"Widget Asset", PinType::String, true, false, 0, "", false, "Widget"});
        node->inputs.push_back({"Element_Name", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Element", PinType::Object, false, true, 0, "", false, "WidgetObjectElement"});
    };
    addWidgetObject.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string containerRef, assetPath, elementName;
        for (const Pin& p : node.inputs) {
            if (p.name == "Container") containerRef = p.defaultValue;
            else if (p.name == "Widget Asset") assetPath = p.defaultValue;
            else if (p.name == "Element_Name") elementName = p.defaultValue;
        }
        if (containerRef.empty() || assetPath.empty()) return;
        WidgetElement* container = ObjectPinRef::ResolveAs<WidgetElement>(node, containerRef, "WidgetElement", ctx);
        if (!container) return;
        WidgetObjectElement* object = new WidgetObjectElement();
        if (!elementName.empty()) object->elementId = elementName;
        container->AddChild(object);
        object->SetWidgetAsset(assetPath);
        for (Pin& p : node.outputs) {
            if (p.name == "Element") p.defaultValue = ObjectPinRef::Encode(object->GetObjectKind(), object->GetObjectId());
        }
    };
    NodeRegistry::Get().RegisterNode(addWidgetObject);

    NodeTemplate clearChildren;
    clearChildren.type = "Clear_Widget_Children";
    clearChildren.category = "Widgets";
    clearChildren.moduleName = "Stargraph";
    clearChildren.menuLabel = "Clear Widget Children";
    clearChildren.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Container", PinType::Object, true, false, 0, "", false, "WidgetElement"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    clearChildren.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string containerRef;
        for (const Pin& p : node.inputs) if (p.name == "Container") containerRef = p.defaultValue;
        if (containerRef.empty()) return;
        WidgetElement* container = ObjectPinRef::ResolveAs<WidgetElement>(node, containerRef, "WidgetElement", ctx);
        if (container) container->ClearChildren();
    };
    NodeRegistry::Get().RegisterNode(clearChildren);


    NodeTemplate setPickerColor;
    setPickerColor.type = "Set_Color_Picker_Color";
    setPickerColor.category = "Widgets";
    setPickerColor.moduleName = "Stargraph";
    setPickerColor.menuLabel = "Set Color Picker Color";
    setPickerColor.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "ColorPickerElement"});
        node->inputs.push_back({"Color (R,G,B,A)", PinType::String, true, false, 0, "255,255,255,255"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setPickerColor.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string targetRef, colorText;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target") targetRef = p.defaultValue;
            else if (p.name == "Color (R,G,B,A)") colorText = p.defaultValue;
        }
        if (targetRef.empty()) return;
        ColorPickerElement* picker = ObjectPinRef::ResolveAs<ColorPickerElement>(node, targetRef, "ColorPickerElement", ctx);
        if (!picker) return;
        int r = 255, g = 255, b = 255, a = 255;
        sscanf(colorText.c_str(), "%d,%d,%d,%d", &r, &g, &b, &a);
        picker->SetColor(Color{ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a });
    };
    NodeRegistry::Get().RegisterNode(setPickerColor);

    NodeTemplate getPickerColor;
    getPickerColor.type = "Get_Color_Picker_Color";
    getPickerColor.category = "Widgets";
    getPickerColor.moduleName = "Stargraph";
    getPickerColor.menuLabel = "Get Color Picker Color";
    getPickerColor.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "ColorPickerElement"});
        node->outputs.push_back({"Color (R,G,B,A)", PinType::String, false, true});
    };
    getPickerColor.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string targetRef;
        for (const Pin& p : node.inputs) if (p.name == "Target") targetRef = p.defaultValue;
        if (targetRef.empty()) return;
        ColorPickerElement* picker = ObjectPinRef::ResolveAs<ColorPickerElement>(node, targetRef, "ColorPickerElement", ctx);
        if (!picker) return;
        char buf[32];
        snprintf(buf, sizeof(buf), "%d,%d,%d,%d", (int)picker->color.r, (int)picker->color.g, (int)picker->color.b, (int)picker->color.a);
        for (Pin& p : node.outputs) if (p.name == "Color (R,G,B,A)") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getPickerColor);

    NodeTemplate callWidgetEvent;
    callWidgetEvent.type = "Call_Widget_Event";
    callWidgetEvent.category = "Widgets";
    callWidgetEvent.moduleName = "Stargraph";
    callWidgetEvent.menuLabel = "Call Widget Event";
    callWidgetEvent.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "WidgetObjectElement"});
        node->inputs.push_back({"Event Name", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Params (Name=Value;...)", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    callWidgetEvent.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string targetRef, eventName, paramText;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target") targetRef = p.defaultValue;
            else if (p.name == "Event Name") eventName = p.defaultValue;
            else if (p.name == "Params (Name=Value;...)") paramText = p.defaultValue;
        }
        if (targetRef.empty() || eventName.empty()) return;
        WidgetObjectElement* target = ObjectPinRef::ResolveAs<WidgetObjectElement>(node, targetRef, "WidgetObjectElement", ctx);
        if (!target || !target->GetChildWidget()) return;
        WidgetBehavior* behavior = target->GetChildWidget()->GetBehavior();
        if (!behavior) return;
        behavior->CallWidgetEvent(target->GetChildWidget(), eventName, ParseWidgetCallArgs(paramText));
    };
    NodeRegistry::Get().RegisterNode(callWidgetEvent);

    NodeTemplate callWidgetFunction;
    callWidgetFunction.type = "Call_Widget_Function";
    callWidgetFunction.category = "Widgets";
    callWidgetFunction.moduleName = "Stargraph";
    callWidgetFunction.menuLabel = "Call Widget Function";
    callWidgetFunction.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "WidgetObjectElement"});
        node->inputs.push_back({"Function Name", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Args (Name=Value;...)", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Result", PinType::String, false, true});
    };
    callWidgetFunction.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string targetRef, functionName, argText;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target") targetRef = p.defaultValue;
            else if (p.name == "Function Name") functionName = p.defaultValue;
            else if (p.name == "Args (Name=Value;...)") argText = p.defaultValue;
        }
        if (targetRef.empty() || functionName.empty()) return;
        WidgetObjectElement* target = ObjectPinRef::ResolveAs<WidgetObjectElement>(node, targetRef, "WidgetObjectElement", ctx);
        if (!target || !target->GetChildWidget()) return;
        WidgetBehavior* behavior = target->GetChildWidget()->GetBehavior();
        if (!behavior) return;
        std::string result;
        if (!behavior->CallWidgetFunction(target->GetChildWidget(), functionName, ParseWidgetCallArgs(argText), result)) return;
        for (Pin& p : node.outputs) {
            if (p.name == "Result") p.defaultValue = result;
        }
    };
    NodeRegistry::Get().RegisterNode(callWidgetFunction);

    NodeTemplate setWidgetVariable;
    setWidgetVariable.type = "Set_Widget_Variable";
    setWidgetVariable.category = "Widgets";
    setWidgetVariable.moduleName = "Stargraph";
    setWidgetVariable.menuLabel = "Set Widget Variable";
    setWidgetVariable.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "WidgetObjectElement"});
        node->inputs.push_back({"Variable_Name", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Value", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setWidgetVariable.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string targetRef, variableName, value;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target") targetRef = p.defaultValue;
            else if (p.name == "Variable_Name") variableName = p.defaultValue;
            else if (p.name == "Value") value = p.defaultValue;
        }
        if (targetRef.empty() || variableName.empty()) return;
        WidgetObjectElement* target = ObjectPinRef::ResolveAs<WidgetObjectElement>(node, targetRef, "WidgetObjectElement", ctx);
        if (!target || !target->GetChildWidget()) return;
        WidgetBehavior* behavior = target->GetChildWidget()->GetBehavior();
        if (!behavior) return;
        behavior->SetWidgetVariable(target->GetChildWidget(), variableName, value);
    };
    NodeRegistry::Get().RegisterNode(setWidgetVariable);

    for (const auto& entry : WidgetElementEvents::GetAll()) {
        ObjectEvents::RegisterWidgetElementEvents(entry.first, entry.second);
    }
}
