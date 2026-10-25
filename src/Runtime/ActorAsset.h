#pragma once
#include "ComponentDecl.h"
#include "Core/SectionFileIO.h"
#include <istream>
#include <ostream>
#include <string>
#include <vector>

class IBehaviorContent;
class ActorBehavior;

struct ActorAssetData {
    std::string assetName;
    std::string parentClass = "Actor";
    std::string programmingInterface;
    std::vector<ComponentDecl> components;
    IBehaviorContent* behaviorContent = nullptr;

    ActorAssetData() = default;
    ActorAssetData(const ActorAssetData&) = delete;
    ActorAssetData& operator=(const ActorAssetData&) = delete;
    ~ActorAssetData();
};

struct ActorAssetHeader {
    std::string assetName;
    std::string parentClass = "Actor";
    std::string programmingInterface;
    int version = 1;
    bool isLegacy = false;
    std::string legacySection;
    bool found = false;
};

namespace ActorAsset {

constexpr const char* ContentPrefix = "Actor.";

bool Load(const std::string& path, ActorAssetData& outData, bool loadBehavior = true);
bool Read(std::istream& in, ActorAssetData& outData, bool loadBehavior = true);
bool Save(const std::string& path, const ActorAssetData& data);
bool Write(std::ostream& out, const ActorAssetData& data);

std::string PeekInterface(const std::string& path);

void WriteHeader(std::ostream& out, const ActorAssetHeader& header);
void WriteComponents(std::ostream& out, const std::vector<ComponentDecl>& components);
ActorAssetHeader ReadHeader(const std::vector<SectionFileIO::ParsedSection>& sections);
void ReadComponents(const std::vector<SectionFileIO::ParsedSection>& sections, std::vector<ComponentDecl>& outComponents);

ActorBehavior* CreateBehavior(const ActorAssetData& data);

}
