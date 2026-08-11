#include "InputComponent.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/GraphExecutor.h"
#include "Modules/Stargraph/InputKeyNames.h"
#include "Core/InputMappingRegistry.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Runtime/PointerUtil.h"
#include "raylib.h"
#include <cstdio>

namespace {

Component* ResolveTargetComponent(Node& node, ExecutionContext& ctx) {
    std::string componentRef;
    for (const Pin& p : node.inputs) {
        if (p.name == "Component") { componentRef = p.defaultValue; break; }
    }

    Component* target = nullptr;
    if (!componentRef.empty()) {
        target = StarPointer::FromHex<Component>(componentRef);
        if (!target && ctx.actor) target = ctx.actor->FindComponentByName(componentRef);
    }
    if (!target && ctx.actor) target = ctx.actor->GetInputComponent();
    return target;
}

bool IsDeviceClaimedByOther(const std::string& device, InputComponent* self) {
    for (const auto& actorPtr : World::Get().GetActors()) {
        InputComponent* ic = actorPtr->GetInputComponent();
        if (ic && ic != self && ic->device == device) return true;
    }
    return false;
}

}

void RegisterInputComponentNodes() {
    NodeTemplate inputMapping;
    inputMapping.type = "Input_Mapping";
    inputMapping.category = "Input";
    inputMapping.moduleName = "Stargraph";
    inputMapping.isEvent = true;
    inputMapping.hideFromContextMenu = true; // listed per mapping name, see RenderNodeContextMenu
    inputMapping.createPins = [](Node* node) {
        node->inputs.push_back({"Component", PinType::Object, true, false, 0, ""});
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
        InputComponent* target = dynamic_cast<InputComponent*>(ResolveTargetComponent(node, ctx));
        if (!target) return;

        std::string mappingName;
        for (const Pin& p : node.inputs) if (p.name == "Mapping") { mappingName = p.defaultValue; break; }

        const InputMapping* mapping = InputMappingRegistry::Get().Find(mappingName);

        bool held = false;
        float axis = 0.0f;
        float axisX = 0.0f;
        float axisY = 0.0f;

        if (mapping) {
            for (const InputMappingBinding& binding : mapping->bindings) {
                int gamepadIndex = 0;
                if (!InputKeyBelongsToDevice(binding.key, target->device, gamepadIndex)) continue;

                float raw = 0.0f;
                if (!PollInputKey(binding.key, raw, gamepadIndex)) continue;

                if (mapping->type == InputMappingType::Bool)       held = true;
                else if (mapping->type == InputMappingType::Axis)  axis += raw * binding.scale;
                else if (binding.axis == 0)                        axisX += raw * binding.scale;
                else                                                axisY += raw * binding.scale;
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

    NodeTemplate overrideInput;
    overrideInput.type = "Override_Input";
    overrideInput.category = "Input";
    overrideInput.moduleName = "Stargraph";
    overrideInput.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Component", PinType::Object, true, false, 0, ""});
        node->inputs.push_back({"Mapping", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"New Key", PinType::String, true, false, 0, "W"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    overrideInput.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!dynamic_cast<InputComponent*>(ResolveTargetComponent(node, ctx))) return;

        std::string mappingName, newKey;
        for (const Pin& p : node.inputs) {
            if (p.name == "Mapping") mappingName = p.defaultValue;
            else if (p.name == "New Key") newKey = p.defaultValue;
        }
        if (mappingName.empty() || newKey.empty()) return;

        InputMapping mapping;
        if (const InputMapping* existing = InputMappingRegistry::Get().Find(mappingName)) mapping = *existing;
        InputMappingBinding binding;
        binding.key = newKey;
        mapping.bindings = {binding};
        InputMappingRegistry::Get().SetMappingRuntime(mappingName, mapping);
    };
    NodeRegistry::Get().RegisterNode(overrideInput);

    NodeTemplate registerUnclaimed;
    registerUnclaimed.type = "Register_Unclaimed_Input";
    registerUnclaimed.category = "Input";
    registerUnclaimed.moduleName = "Stargraph";
    registerUnclaimed.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Component", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Claimed", PinType::Exec, false, true});
        node->outputs.push_back({"Unclaimed", PinType::Exec, false, true});
    };
    registerUnclaimed.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        InputComponent* target = dynamic_cast<InputComponent*>(ResolveTargetComponent(node, ctx));
        if (!target) { GraphExecutor::RunFromPin(graph, &node, ctx, "Unclaimed"); return; }

        if (target->device == "Unclaimed" && !IsDeviceClaimedByOther("Keyboard+Mouse", target)) {
            for (const std::string& key : GetInputKeyNames()) {
                if (key.rfind("GAMEPAD_", 0) == 0 || InputKeyIsAnalog(key)) continue;
                float axis = 0.0f;
                if (PollInputKey(key, axis)) { target->device = "Keyboard+Mouse"; break; }
            }
        }

        if (target->device == "Unclaimed") {
            for (int i = 0; i < 4 && target->device == "Unclaimed"; i++) {
                if (!IsGamepadAvailable(i)) continue;
                std::string candidate = "Gamepad " + std::to_string(i);
                if (IsDeviceClaimedByOther(candidate, target)) continue;

                for (const std::string& key : GetInputKeyNames()) {
                    if (key.rfind("GAMEPAD_", 0) != 0 || InputKeyIsAnalog(key)) continue;
                    float axis = 0.0f;
                    if (PollInputKey(key, axis, i)) { target->device = candidate; break; }
                }
            }
        }

        GraphExecutor::RunFromPin(graph, &node, ctx, target->device == "Unclaimed" ? "Unclaimed" : "Claimed");
    };
    NodeRegistry::Get().RegisterNode(registerUnclaimed);
}

void InputComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}