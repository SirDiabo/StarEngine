#include "MeshComponent.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Modules/Stargraph/Stargraph.h"
#include <cstdlib>

void RegisterMeshComponentNodes() {
    NodeTemplate setStaticMesh;
    setStaticMesh.type = "Set_Static_Mesh";
    setStaticMesh.category = "Mesh";
    setStaticMesh.moduleName = "Stargraph";
    setStaticMesh.hideFromContextMenu = true;
    setStaticMesh.requiresComponentClass = "MeshComponent";
    setStaticMesh.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Static Mesh", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setStaticMesh.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
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
    setMaterial.requiresComponentClass = "MeshComponent";
    setMaterial.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false});
        node->inputs.push_back({"Material", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setMaterial.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Component* target = nullptr;
        std::string path;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Component*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Material") path = p.defaultValue;
        }
        if (!target) return;
        if (MeshComponent* mc = dynamic_cast<MeshComponent*>(target)) {
            mc->SetMaterialPath(0, path);
        }
    };
    NodeRegistry::Get().RegisterNode(setMaterial);
}

void MeshComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddString("Static Mesh", &meshPath);
}

void MeshComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}

void MeshComponent::SetMeshPath(const std::string& path) {
    meshPath = path;
}

void MeshComponent::SetMaterialPaths(const std::vector<std::string>& paths) {
    materialPaths = paths;
}

void MeshComponent::SetMaterialPath(int slotIndex, const std::string& path) {
    if (slotIndex < 0) return;
    if ((size_t)slotIndex >= materialPaths.size()) materialPaths.resize(slotIndex + 1);
    materialPaths[slotIndex] = path;
}