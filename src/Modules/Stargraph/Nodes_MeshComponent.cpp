#include "Runtime/Components/MeshComponent.h"
#include "NodeRegistry.h"
#include "Stargraph.h"
#include "Core/StarAsset.h"
#include "Runtime/AssetCache.h"
#include <cmath>
#include <cstdlib>
#include "ObjectPinRef.h"
#include "Nodes_Components.h"

void RegisterMeshComponentNodes() {
    NodeTemplate setStaticMesh;
    setStaticMesh.type = "Set_Static_Mesh";
    setStaticMesh.category = "Mesh";
    setStaticMesh.moduleName = "Stargraph";
    setStaticMesh.hideFromContextMenu = true;
    setStaticMesh.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "MeshComponent"});
        node->inputs.push_back({"Static Mesh", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setStaticMesh.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "MeshComponent", ctx);
            }
            if (p.name == "Static Mesh") path = p.defaultValue;
        }
        if (!target) return;
        if (MeshComponent* mc = dynamic_cast<MeshComponent*>(target)) {
            mc->SetMeshPath(path);
        }
    };
    NodeRegistry::Get().RegisterNode(setStaticMesh);

    NodeTemplate setMaterial;
    setMaterial.type = "Set_Material";
    setMaterial.category = "Mesh";
    setMaterial.moduleName = "Stargraph";
    setMaterial.hideFromContextMenu = true;
    setMaterial.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "MeshComponent"});
        node->inputs.push_back({"Material", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setMaterial.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "MeshComponent", ctx);
            }
            if (p.name == "Material") path = p.defaultValue;
        }
        if (!target) return;
        if (MeshComponent* mc = dynamic_cast<MeshComponent*>(target)) {
            mc->SetMaterialPath(0, path);
        }
    };
    NodeRegistry::Get().RegisterNode(setMaterial);

    NodeTemplate playAnim;
    playAnim.type = "Play_Animation";
    playAnim.category = "Mesh";
    playAnim.moduleName = "Stargraph";
    playAnim.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "MeshComponent"});
        node->inputs.push_back({"Anim", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Looping", PinType::Boolean, true, false, 0, "false"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    playAnim.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        std::string animPath;
        bool looping = false;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "MeshComponent", ctx);
            }
            if (p.name == "Anim") animPath = p.defaultValue;
            if (p.name == "Looping") looping = p.defaultValue.AsBool();
        }
        if (!target) return;
        if (MeshComponent* mc = dynamic_cast<MeshComponent*>(target)) {
            mc->PlayAnimation(animPath, looping);
        }
    };
    NodeRegistry::Get().RegisterNode(playAnim);

    NodeTemplate stopAnim;
    stopAnim.type = "Stop_Animation";
    stopAnim.category = "Mesh";
    stopAnim.moduleName = "Stargraph";
    stopAnim.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "MeshComponent"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    stopAnim.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "MeshComponent", ctx);
            }
        }
        if (!target) return;
        if (MeshComponent* mc = dynamic_cast<MeshComponent*>(target)) {
            mc->StopAnimation();
        }
    };
    NodeRegistry::Get().RegisterNode(stopAnim);

    NodeTemplate setAnimRate;
    setAnimRate.type = "Set_Anim_Play_Rate";
    setAnimRate.category = "Mesh";
    setAnimRate.moduleName = "Stargraph";
    setAnimRate.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, "", false, "MeshComponent"});
        node->inputs.push_back({"Rate", PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setAnimRate.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        float rate = 1.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = ObjectPinRef::ResolveAs<Component>(node, p.defaultValue, "MeshComponent", ctx);
            }
            if (p.name == "Rate") rate = p.defaultValue.AsFloat();
        }
        if (!target) return;
        if (MeshComponent* mc = dynamic_cast<MeshComponent*>(target)) {
            mc->SetAnimPlayRate(rate);
        }
    };
    NodeRegistry::Get().RegisterNode(setAnimRate);
}
