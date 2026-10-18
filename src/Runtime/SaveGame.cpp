#include "SaveGame.h"
#include "Actor.h"
#include "World.h"
#include "GraphBehavior.h"
#include "Components/Component.h"
#include "Property.h"
#include "Core/SaveFileFormat.h"
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>

namespace {

using EntryMap = std::unordered_map<std::string, std::string>;
using FileEntries = std::unordered_map<std::string, EntryMap>;

const char kArraySeparator = '\x1f';

std::string FloatToString(float value) {
    char buf[32];
    for (int precision = 6; precision <= 9; precision++) {
        snprintf(buf, sizeof(buf), "%.*g", precision, value);
        if (strtof(buf, nullptr) == value) break;
    }
    return buf;
}

std::string PropertyToString(const PropertyHandle& h) {
    switch (h.type) {
        case PropertyType::Float: return FloatToString(h.AsFloat());
        case PropertyType::Int: return std::to_string(h.AsInt());
        case PropertyType::Bool: return h.AsBool() ? "true" : "false";
        case PropertyType::String: return h.AsString();
        case PropertyType::Vector3: {
            float* v = h.AsVector3();
            return FloatToString(v[0]) + "," + FloatToString(v[1]) + "," + FloatToString(v[2]);
        }
    }
    return "";
}

void PropertyFromString(const PropertyHandle& h, const std::string& text) {
    switch (h.type) {
        case PropertyType::Float:  h.AsFloat() = strtof(text.c_str(), nullptr); break;
        case PropertyType::Int:    h.AsInt() = atoi(text.c_str()); break;
        case PropertyType::Bool:   h.AsBool() = (text == "true" || text == "1"); break;
        case PropertyType::String: h.AsString() = text; break;
        case PropertyType::Vector3: {
            float* v = h.AsVector3();
            sscanf(text.c_str(), "%f,%f,%f", &v[0], &v[1], &v[2]);
            break;
        }
    }
}

std::string EscapeElement(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == kArraySeparator) out += "\\s";
        else out += c;
    }
    return out;
}

std::string UnescapeElement(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            char n = text[++i];
            if (n == 'n') out += '\n';
            else if (n == 'r') out += '\r';
            else if (n == 's') out += kArraySeparator;
            else out += n;
        } else {
            out += text[i];
        }
    }
    return out;
}

std::string JoinArray(const std::vector<std::string>& values) {
    std::string out;
    for (size_t i = 0; i < values.size(); i++) {
        if (i) out += kArraySeparator;
        out += EscapeElement(values[i]);
    }
    return out;
}

std::vector<std::string> SplitArray(const std::string& text) {
    std::vector<std::string> out;
    if (text.empty()) return out;
    size_t start = 0;
    while (true) {
        size_t at = text.find(kArraySeparator, start);
        if (at == std::string::npos) { out.push_back(UnescapeElement(text.substr(start))); break; }
        out.push_back(UnescapeElement(text.substr(start, at - start)));
        start = at + 1;
    }
    return out;
}

std::string VariableKey(const Actor* actor, const std::string& name, bool isArray) {
    return actor->GetName() + "." + name + (isArray ? "[]" : "");
}

std::string ComponentFieldKey(const Actor* actor, const Component* comp, const std::string& fieldName) {
    return actor->GetName() + "." + comp->GetName() + "." + fieldName;
}

bool Wanted(const std::string& file, const std::string& onlyFile) {
    return !file.empty() && (onlyFile.empty() || file == onlyFile);
}

void CollectActor(Actor* actor, const std::string& onlyFile, FileEntries& out) {
    if (GraphBehavior* behavior = actor->GetGraphBehavior()) {
        std::vector<SavableVariable> variables;
        behavior->CollectSavableVariables(variables);
        for (const SavableVariable& v : variables) {
            if (!Wanted(v.saveFile, onlyFile)) continue;
            out[v.saveFile][VariableKey(actor, v.name, v.isArray)] = v.isArray ? JoinArray(v.arrayValues) : v.value;
        }
    }
    for (Component* comp : actor->GetComponents()) {
        PropertyList props;
        comp->GetProperties(props);
        for (const PropertyHandle& h : props.GetAll()) {
            if (h.readOnly) continue;
            auto it = comp->savableFields.find(h.name);
            if (it == comp->savableFields.end() || !Wanted(it->second, onlyFile)) continue;
            out[it->second][ComponentFieldKey(actor, comp, h.name)] = PropertyToString(h);
        }
    }
}

void WriteAll(const FileEntries& byFile) {
    for (const auto& kv : byFile) SaveFileFormat::WriteEntries(kv.first, kv.second);
}

const EntryMap& EntriesFor(FileEntries& cache, const std::string& file) {
    auto it = cache.find(file);
    if (it != cache.end()) return it->second;
    return cache[file] = SaveFileFormat::ReadEntries(file);
}

void ApplyActor(Actor* actor, const std::string& onlyFile, FileEntries& cache) {
    if (GraphBehavior* behavior = actor->GetGraphBehavior()) {
        std::vector<SavableVariable> variables;
        behavior->CollectSavableVariables(variables);
        for (const SavableVariable& v : variables) {
            if (!Wanted(v.saveFile, onlyFile)) continue;
            const EntryMap& entries = EntriesFor(cache, v.saveFile);
            auto it = entries.find(VariableKey(actor, v.name, v.isArray));
            if (it == entries.end()) continue;
            SavableVariable loaded = v;
            if (v.isArray) loaded.arrayValues = SplitArray(it->second);
            else loaded.value = it->second;
            behavior->ApplySavedVariable(loaded);
        }
    }
    for (Component* comp : actor->GetComponents()) {
        PropertyList props;
        comp->GetProperties(props);
        for (const PropertyHandle& h : props.GetAll()) {
            if (h.readOnly) continue;
            auto sit = comp->savableFields.find(h.name);
            if (sit == comp->savableFields.end() || !Wanted(sit->second, onlyFile)) continue;
            const EntryMap& entries = EntriesFor(cache, sit->second);
            auto eit = entries.find(ComponentFieldKey(actor, comp, h.name));
            if (eit != entries.end()) PropertyFromString(h, eit->second);
        }
    }
}

}

namespace SaveGame {

void SaveActor(Actor* actor, const std::string& onlyFile) {
    if (!actor) return;
    FileEntries byFile;
    CollectActor(actor, onlyFile, byFile);
    WriteAll(byFile);
}

void LoadActor(Actor* actor, const std::string& onlyFile) {
    if (!actor) return;
    FileEntries cache;
    ApplyActor(actor, onlyFile, cache);
}

void SaveWorld(const std::string& onlyFile) {
    FileEntries byFile;
    for (const auto& actor : World::Get().GetActors()) CollectActor(actor.get(), onlyFile, byFile);
    WriteAll(byFile);
}

void LoadWorld(const std::string& onlyFile) {
    FileEntries cache;
    for (const auto& actor : World::Get().GetActors()) ApplyActor(actor.get(), onlyFile, cache);
}

bool SaveVariable(Actor* actor, const std::string& variableName, const std::string& file) {
    if (!actor || !actor->GetGraphBehavior()) return false;
    std::vector<SavableVariable> variables;
    actor->GetGraphBehavior()->CollectSavableVariables(variables);
    for (const SavableVariable& v : variables) {
        if (v.name != variableName) continue;
        std::string target = !file.empty() ? file : v.saveFile;
        if (target.empty()) return false;
        return SaveFileFormat::WriteEntries(target, {{ VariableKey(actor, v.name, v.isArray),
                                                       v.isArray ? JoinArray(v.arrayValues) : v.value }});
    }
    return false;
}

bool LoadVariable(Actor* actor, const std::string& variableName, const std::string& file) {
    if (!actor || !actor->GetGraphBehavior()) return false;
    GraphBehavior* behavior = actor->GetGraphBehavior();
    std::vector<SavableVariable> variables;
    behavior->CollectSavableVariables(variables);
    for (const SavableVariable& v : variables) {
        if (v.name != variableName) continue;
        std::string source = !file.empty() ? file : v.saveFile;
        if (source.empty()) return false;
        EntryMap entries = SaveFileFormat::ReadEntries(source);
        auto it = entries.find(VariableKey(actor, v.name, v.isArray));
        if (it == entries.end()) return false;
        SavableVariable loaded = v;
        if (v.isArray) loaded.arrayValues = SplitArray(it->second);
        else loaded.value = it->second;
        behavior->ApplySavedVariable(loaded);
        return true;
    }
    return false;
}

}
