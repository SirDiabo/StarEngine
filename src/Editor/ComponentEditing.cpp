#include "ComponentEditing.h"
#include "Runtime/Components/Component.h"
#include <map>

LiveComponentCache::~LiveComponentCache() {
    Clear();
}

void LiveComponentCache::Clear() {
    for (auto& [key, entry] : entries) delete entry.component;
    entries.clear();
}

static Component* BuildLive(const ComponentTypeInfo& info, const ComponentDecl& decl) {
    if (!info.factory) return nullptr;
    Component* component = info.factory();
    if (!component) return nullptr;
    ComponentRegistry::EditorApplyScope scope;
    for (const ComponentFieldDesc& field : info.fields) {
        if (field.apply) field.apply(*component, decl);
    }
    if (info.applyArrays) info.applyArrays(*component, decl);
    return component;
}

Component* LiveComponentCache::Acquire(const std::string& key, const ComponentDecl& decl) {
    const ComponentTypeInfo* info = ComponentRegistry::Get().Find(decl.componentType);
    if (!info) return nullptr;

    std::string signature = ComponentEditing::DeclSignature(decl);
    Entry& entry = entries[key];
    if (entry.component && entry.componentType == decl.componentType && entry.signature == signature) {
        return entry.component;
    }

    delete entry.component;
    entry.component = BuildLive(*info, decl);
    entry.componentType = decl.componentType;
    entry.signature = signature;
    return entry.component;
}

void LiveComponentCache::MarkInSync(const std::string& key, const ComponentDecl& decl) {
    auto it = entries.find(key);
    if (it != entries.end()) it->second.signature = ComponentEditing::DeclSignature(decl);
}

namespace ComponentEditing {

std::string DeclSignature(const ComponentDecl& decl) {
    std::map<std::string, std::string> sortedFields(decl.fields.begin(), decl.fields.end());
    std::map<std::string, std::vector<std::string>> sortedArrays(decl.arrayFields.begin(), decl.arrayFields.end());
    std::string out = decl.componentType;
    for (const auto& [k, v] : sortedFields) { out += '\n'; out += k; out += '='; out += v; }
    for (const auto& [k, values] : sortedArrays) {
        out += '\n'; out += k; out += '[';
        for (const std::string& v : values) { out += v; out += '\x1f'; }
        out += ']';
    }
    return out;
}

static const ComponentFieldDesc* FindField(const ComponentTypeInfo* info, const std::string& fieldKey) {
    if (!info) return nullptr;
    for (const ComponentFieldDesc& f : info->fields) {
        if (f.key == fieldKey) return &f;
    }
    return nullptr;
}

std::string ReadField(LiveComponentCache& cache, const std::string& key, const ComponentDecl& decl,
                      const ComponentFieldDesc& field) {
    if (field.read) {
        if (Component* live = cache.Acquire(key, decl)) return field.read(*live);
    }
    return decl.Get(field.key, field.defaultValue);
}

void WriteField(LiveComponentCache& cache, const std::string& key, ComponentDecl& decl,
                const std::string& fieldKey, const std::string& value) {
    const ComponentTypeInfo* info = ComponentRegistry::Get().Find(decl.componentType);
    const ComponentFieldDesc* field = FindField(info, fieldKey);
    Component* live = (field && field->apply && field->read) ? cache.Acquire(key, decl) : nullptr;
    if (!live) {
        decl.fields[fieldKey] = value;
        return;
    }

    ComponentDecl patched = decl;
    patched.fields[fieldKey] = value;
    {
        ComponentRegistry::EditorApplyScope scope;
        field->apply(*live, patched);
    }
    decl.fields[fieldKey] = field->read(*live);
    cache.MarkInSync(key, decl);
}

void WriteArray(LiveComponentCache& cache, const std::string& key, ComponentDecl& decl,
                const std::string& arrayKey, const std::vector<std::string>& values) {
    const ComponentTypeInfo* info = ComponentRegistry::Get().Find(decl.componentType);
    Component* live = (info && info->applyArrays && info->readArrays) ? cache.Acquire(key, decl) : nullptr;
    if (!live) {
        decl.arrayFields[arrayKey] = values;
        return;
    }

    ComponentDecl patched = decl;
    patched.arrayFields[arrayKey] = values;
    {
        ComponentRegistry::EditorApplyScope scope;
        info->applyArrays(*live, patched);
    }
    info->readArrays(*live, decl);
    cache.MarkInSync(key, decl);
}

}
