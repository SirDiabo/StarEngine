#include "InputComponent.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/InputKeyNames.h"
#include "Core/InputMappingRegistry.h"
#include <cstdio>

void RegisterInputComponentNodes() {
    NodeTemplate inputMapping;
    inputMapping.type = "Input_Mapping";
    inputMapping.category = "Input";
    inputMapping.moduleName = "Stargraph";
    inputMapping.isEvent = true;
    inputMapping.hideFromContextMenu = true; // listed per mapping name, see RenderNodeContextMenu
    inputMapping.createPins = [](Node* node) {
        node->inputs.push_back({"Mapping", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Triggered", PinType::Exec, false, true});
        node->outputs.push_back({"Started", PinType::Exec, false, true});
        node->outputs.push_back({"Completed", PinType::Exec, false, true});
        node->outputs.push_back({"Is Held", PinType::Boolean, false, true});
        node->outputs.push_back({"Axis", PinType::Float, false, true});
        node->outputs.push_back({"Axis 2D", PinType::Vec3, false, true});
        node->outputs.push_back({"Input Time", PinType::Float, false, true});
    };
    inputMapping.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string mappingName;
        for (const Pin& p : node.inputs) if (p.name == "Mapping") mappingName = p.defaultValue;

        const InputMapping* mapping = InputMappingRegistry::Get().Find(mappingName);

        bool held = false;
        float axis = 0.0f;
        float axisX = 0.0f;
        float axisY = 0.0f;

        if (mapping) {
            for (const InputMappingBinding& binding : mapping->bindings) {
                float raw = 0.0f;
                if (!PollInputKey(binding.key, raw)) continue;

                if (mapping->type == InputMappingType::Bool)       held = true;
                else if (mapping->type == InputMappingType::Axis)  axis += binding.scale;
                else if (binding.axis == 0)                        axisX += binding.scale;
                else                                                axisY += binding.scale;
            }
            if (mapping->type == InputMappingType::Axis)   held = axis != 0.0f;
            if (mapping->type == InputMappingType::Axis2D) held = (axisX != 0.0f || axisY != 0.0f);
        }

        float inputTime = held ? (ctx.deltaTime * 120.0f) : 0.0f;
        char axisBuf[32];  snprintf(axisBuf, sizeof(axisBuf), "%g", axis);
        char axis2DBuf[64]; snprintf(axis2DBuf, sizeof(axis2DBuf), "%g,%g,0", axisX, axisY);
        char timeBuf[32];  snprintf(timeBuf, sizeof(timeBuf), "%g", inputTime);

        for (Pin& p : node.outputs) {
            if (p.name == "Is Held")        p.defaultValue = held ? "true" : "false";
            else if (p.name == "Axis")      p.defaultValue = axisBuf;
            else if (p.name == "Axis 2D")   p.defaultValue = axis2DBuf;
            else if (p.name == "Input Time") p.defaultValue = timeBuf;
        }
    };
    NodeRegistry::Get().RegisterNode(inputMapping);
}

void InputComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}