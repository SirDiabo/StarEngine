#include "StarsequenceModule.h"
#include "SequenceAudioSource.h"
#include "SequenceFormat.h"
#include "Core/AssetTypeRegistry.h"
#include "Runtime/Audio/AudioSource.h"
#include <filesystem>

StarsequenceModule::StarsequenceModule() {
    name = "Starsequence";
}

bool StarsequenceModule::Initialize() {
    AudioSourceType sourceType;
    sourceType.extension = ".starseq";
    sourceType.displayName = "Starsequence";
    sourceType.moduleName = "Starsequence";
    sourceType.create = [](const std::string& assetPath) -> std::unique_ptr<AudioSource> {
        auto source = std::make_unique<Starsequence::SequenceAudioSource>(assetPath);
        if (!source->IsValid()) return nullptr;
        return source;
    };
    sourceType.describeParameters = [](const std::string& assetPath, std::vector<AudioParamInfo>& out) {
        Starsequence::ParseResult parsed;
        if (Starsequence::LoadSequenceFile(assetPath, parsed)) Starsequence::DescribeSequenceParameters(parsed.sequence, out);
    };
    AudioSourceRegistry::Get().Register(sourceType);

    AssetTypeInfo assetType;
    assetType.typeName = "Starsequence";
    assetType.extension = ".starseq";
    assetType.category = "Audio";
    assetType.moduleName = "Starsequence";
    assetType.createAsset = [](const std::string& fullPath, const std::string& assetName) {
        std::string error;
        return Starsequence::SaveSequenceFile(fullPath, Starsequence::MakeDefaultSequence(assetName), error);
    };
    assetType.renameAsset = [](const std::string& fullPath, const std::string& newAssetName) {
        Starsequence::ParseResult parsed;
        if (!Starsequence::LoadSequenceFile(fullPath, parsed)) return false;
        parsed.sequence.name = newAssetName;
        std::string error;
        return Starsequence::SaveSequenceFile(fullPath, parsed.sequence, error);
    };
    AssetTypeRegistry::Get().RegisterType(assetType);
    return true;
}

void StarsequenceModule::Shutdown() {}

void StarsequenceModule::Update(float) {}
