#include "MeshesModule.h"
#include "Core/AssetTypeRegistry.h"
#include "Core/StarMaterial.h"

MeshesModule::MeshesModule() {
    name = "Meshes";
}

bool MeshesModule::Initialize() {
    RegisterAssetType();
    return true;
}

void MeshesModule::Shutdown() {}
void MeshesModule::Update(float deltaTime) {}

void MeshesModule::RegisterAssetType() {
    AssetTypeInfo mesh;
    mesh.typeName = "StaticMesh";
    mesh.extension = ".starasset";
    mesh.category = "Mesh";
    mesh.moduleName = "Meshes";
    AssetTypeRegistry::Get().RegisterType(mesh);

    AssetTypeInfo texture;
    texture.typeName = "Texture";
    texture.extension = ".startex";
    texture.category = "Mesh";
    texture.moduleName = "Meshes";
    AssetTypeRegistry::Get().RegisterType(texture);

    AssetTypeInfo material;
    material.typeName = "Material";
    material.extension = ".starmat";
    material.category = "Mesh";
    material.moduleName = "Meshes";
    material.createAsset = [](const std::string& fullPath, const std::string& assetName) {
        StarMaterial mat;
        return mat.SaveToFile(fullPath);
    };
    AssetTypeRegistry::Get().RegisterType(material);

    AssetTypeInfo animation;
    animation.typeName = "Animation";
    animation.extension = ".staranim";
    animation.category = "Mesh";
    animation.moduleName = "Meshes";
    AssetTypeRegistry::Get().RegisterType(animation);
}
