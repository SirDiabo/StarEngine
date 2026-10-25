#include "ActorAssetSupport.h"
#include "Core/AssetTypeRegistry.h"
#include "Core/EditorAssetHandlerRegistry.h"
#include "Core/ProgrammingInterfaceRegistry.h"
#include "Runtime/ActorAsset.h"
#include "Runtime/ActorClassCache.h"
#include "raylib.h"

namespace {

bool CreateActorAsset(const std::string& fullPath, const std::string& assetName, const std::string& interfaceName) {
    const ProgrammingInterfaceInfo* info = ProgrammingInterfaceRegistry::Get().Find(interfaceName);
    if (!info || !info->SupportsActors() || !info->IsAvailable() || !info->createActorEditorTab) return false;

    ActorAssetData data;
    data.assetName = assetName;
    data.parentClass = "Actor";
    data.programmingInterface = info->name;
    if (info->createActorContent) data.behaviorContent = info->createActorContent();
    return ActorAsset::Save(fullPath, data);
}

}

void RegisterActorAssetSupport() {
    AssetTypeInfo actorType;
    actorType.typeName = "Actor";
    actorType.extension = ".staractor";
    actorType.category = "Gameplay";

    actorType.listCreateVariants = []() {
        std::vector<AssetCreateVariant> variants;
        for (const ProgrammingInterfaceInfo* info : ProgrammingInterfaceRegistry::Get().GetAvailableForActors()) {
            if (info->createActorEditorTab) variants.push_back({ info->name, info->name });
        }
        return variants;
    };

    actorType.createAssetVariant = [](const std::string& fullPath, const std::string& assetName, const std::string& variantId) {
        return CreateActorAsset(fullPath, assetName, variantId);
    };

    actorType.createAsset = [](const std::string& fullPath, const std::string& assetName) {
        std::vector<const ProgrammingInterfaceInfo*> available;
        for (const ProgrammingInterfaceInfo* info : ProgrammingInterfaceRegistry::Get().GetAvailableForActors()) {
            if (info->createActorEditorTab) available.push_back(info);
        }
        if (available.size() != 1) return false;
        return CreateActorAsset(fullPath, assetName, available[0]->name);
    };

    actorType.renameAsset = [](const std::string& fullPath, const std::string& newAssetName) {
        ActorAssetData data;
        if (!ActorAsset::Load(fullPath, data)) return false;
        if (!data.programmingInterface.empty()) {
            const ProgrammingInterfaceInfo* info = ProgrammingInterfaceRegistry::Get().Find(data.programmingInterface);
            if (!info || !info->IsAvailable() || !data.behaviorContent) return false;
        }
        data.assetName = newAssetName;
        bool saved = ActorAsset::Save(fullPath, data);
        if (saved) ActorClassCache::Get().Invalidate(fullPath);
        return saved;
    };
    AssetTypeRegistry::Get().RegisterType(actorType);

    EditorAssetHandlerInfo actorHandler;
    actorHandler.extension = ".staractor";
    actorHandler.createTab = [](const std::string& fullPath, const EditorServices& services) -> EditorTab* {
        std::string interfaceName = ActorAsset::PeekInterface(fullPath);
        const ProgrammingInterfaceInfo* info = ProgrammingInterfaceRegistry::Get().Find(interfaceName);
        if (!info || !info->createActorEditorTab || !info->IsAvailable()) {
            TraceLog(LOG_WARNING, "Cannot open \"%s\": programming interface \"%s\" is not available (is its module enabled?)",
                     fullPath.c_str(), interfaceName.c_str());
            return nullptr;
        }
        return info->createActorEditorTab(fullPath, services);
    };
    EditorAssetHandlerRegistry::Get().RegisterHandler(actorHandler);
}
