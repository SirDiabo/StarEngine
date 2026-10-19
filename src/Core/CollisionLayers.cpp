#include "CollisionLayers.h"
#include "LayoutSerializer.h"
#include "SafeParse.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

struct State {
    std::string names[CollisionLayers::kMaxLayers];
    std::string filePath;
};

State& Data() {
    static State s;
    return s;
}

void ApplyDefaults(State& s) {
    for (std::string& n : s.names) n.clear();
    s.names[0] = "Default";
    s.names[1] = "World";
    s.names[2] = "Actor";
    s.names[3] = "Trigger";
    s.names[4] = "Visibility";
    s.names[5] = "Interaction";
}

bool NameTaken(const State& s, const std::string& name, int except) {
    for (int i = 0; i < CollisionLayers::kMaxLayers; i++) {
        if (i != except && s.names[i] == name) return true;
    }
    return false;
}

}

void CollisionLayers::Load(const std::string& projectRoot) {
    State& s = Data();
    s.filePath = (fs::path(projectRoot) / "Config" / "CollisionLayers.cfg").string();
    ApplyDefaults(s);
    std::unordered_map<std::string, std::string> raw;
    if (!LayoutSerializer::Load(s.filePath, raw)) return;
    for (std::string& n : s.names) n.clear();
    for (int i = 0; i < kMaxLayers; i++) {
        auto it = raw.find("slot." + std::to_string(i));
        if (it != raw.end() && !it->second.empty() && !NameTaken(s, it->second, i)) s.names[i] = it->second;
    }
    if (s.names[0].empty()) s.names[0] = "Default";
}

void CollisionLayers::Save() {
    State& s = Data();
    if (s.filePath.empty()) return;
    std::unordered_map<std::string, std::string> raw;
    for (int i = 0; i < kMaxLayers; i++) {
        if (!s.names[i].empty()) raw["slot." + std::to_string(i)] = s.names[i];
    }
    LayoutSerializer::Save(s.filePath, raw);
}

const std::string& CollisionLayers::NameAt(int index) {
    static const std::string empty;
    if (index < 0 || index >= kMaxLayers) return empty;
    State& s = Data();
    if (s.names[0].empty()) ApplyDefaults(s);
    return s.names[index];
}

int CollisionLayers::IndexOf(const std::string& name) {
    if (name.empty()) return -1;
    for (int i = 0; i < kMaxLayers; i++) {
        if (NameAt(i) == name) return i;
    }
    return -1;
}

std::vector<int> CollisionLayers::UsedIndices() {
    std::vector<int> out;
    for (int i = 0; i < kMaxLayers; i++) {
        if (!NameAt(i).empty()) out.push_back(i);
    }
    return out;
}

std::vector<std::string> CollisionLayers::Names() {
    std::vector<std::string> out;
    for (int i : UsedIndices()) out.push_back(NameAt(i));
    return out;
}

int CollisionLayers::Add(const std::string& name) {
    State& s = Data();
    NameAt(0);
    if (name.empty() || NameTaken(s, name, -1)) return -1;
    for (int i = 0; i < kMaxLayers; i++) {
        if (s.names[i].empty()) {
            s.names[i] = name;
            return i;
        }
    }
    return -1;
}

bool CollisionLayers::Rename(int index, const std::string& name) {
    State& s = Data();
    NameAt(0);
    if (index < 0 || index >= kMaxLayers || s.names[index].empty()) return false;
    if (name.empty() || NameTaken(s, name, index)) return false;
    s.names[index] = name;
    return true;
}

bool CollisionLayers::Remove(int index) {
    State& s = Data();
    NameAt(0);
    if (index <= 0 || index >= kMaxLayers || s.names[index].empty()) return false;
    s.names[index].clear();
    return true;
}

uint32_t CollisionLayers::ParseMask(const std::string& text) {
    if (text.empty()) return kAllMask;
    char* end = nullptr;
    unsigned long long value = std::strtoull(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') return kAllMask;
    return (uint32_t)value;
}

std::string CollisionLayers::MaskToString(uint32_t mask) {
    return std::to_string((unsigned long long)mask);
}
