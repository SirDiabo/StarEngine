#include "MeshesModule.h"
#include "Core/AssetTypeRegistry.h"

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
    AssetTypeInfo info;
    info.typeName = "StaticMesh";
    info.extension = ".starasset";
    info.category = "Mesh";
    info.moduleName = "Meshes";
    AssetTypeRegistry::Get().RegisterType(info);
}
