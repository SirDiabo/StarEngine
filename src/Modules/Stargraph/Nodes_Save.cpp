#include "Nodes_Save.h"
#include "NodeRegistry.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/Component.h"
#include "Runtime/Property.h"
#include "Core/SaveFileFormat.h"
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace {
    std::string PropertyToString(const PropertyHandle& h) {
        switch (h.type) {
            case PropertyType::Float: {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.2f", h.AsFloat());
                return buf;
            }
            case PropertyType::Int: return std::to_string(h.AsInt());
            case PropertyType::Bool: return h.AsBool() ? "true" : "false";
            case PropertyType::String: return h.AsString();
            case PropertyType::Vector3: {
                float* v = h.AsVector3();
                char buf[64];
                snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f", v[0], v[1], v[2]);
                return buf;
            }
        }
        return "";
    }

    void PropertyFromString(const PropertyHandle& h, const std::string& text) {
        switch (h.type) {
            case PropertyType::Float:  h.AsFloat() = (float)atof(text.c_str()); break;
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

    std::string VariableKey(Actor* actor, const std::string& varName) {
        return actor->GetName() + "." + varName;
    }

    std::string ComponentFieldKey(Actor* actor, Component* comp, const std::string& fieldName) {
        return actor->GetName() + "." + comp->GetName() + "." + fieldName;
    }

    std::string InputValue(const Node& node, const std::string& pinName) {
        for (const Pin& p : node.inputs) if (p.name == pinName) return p.defaultValue;
        return "";
    }
}

void RegisterSaveNodes() {
    NodeTemplate saveVar;
    saveVar.type = "Save_Variable";
    saveVar.category = "Save";
    saveVar.moduleName = "Stargraph";
    saveVar.createPins = [](Node* node) {
        node->inputs.push_back({"In",            PinType::Exec,   true,  false});
        node->inputs.push_back({"Variable_Name", PinType::String, true,  false, 0, ""});
        node->inputs.push_back({"Target File",   PinType::String, true,  false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    saveVar.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        std::string varName = InputValue(node, "Variable_Name");
        Variable* var = graph.FindVariable(varName);
        if (!var) return;
        std::string targetFile = InputValue(node, "Target File");
        std::string file = !targetFile.empty() ? targetFile : var->saveFile;
        if (file.empty()) return;
        SaveFileFormat::WriteEntries(file, {{VariableKey(ctx.actor, varName), var->value}});
    };
    NodeRegistry::Get().RegisterNode(saveVar);

    NodeTemplate saveAll;
    saveAll.type = "Save_All";
    saveAll.category = "Save";
    saveAll.moduleName = "Stargraph";
    saveAll.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    saveAll.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> byFile;

        for (const Variable& v : graph.GetVariables()) {
            if (v.saveFile.empty()) continue;
            byFile[v.saveFile][VariableKey(ctx.actor, v.name)] = v.value;
        }
        for (Component* comp : ctx.actor->GetComponents()) {
            PropertyList props;
            comp->GetProperties(props);
            for (const PropertyHandle& h : props.GetAll()) {
                if (h.readOnly) continue;
                auto it = comp->savableFields.find(h.name);
                if (it == comp->savableFields.end() || it->second.empty()) continue;
                byFile[it->second][ComponentFieldKey(ctx.actor, comp, h.name)] = PropertyToString(h);
            }
        }
        for (const auto& kv : byFile) SaveFileFormat::WriteEntries(kv.first, kv.second);
    };
    NodeRegistry::Get().RegisterNode(saveAll);

    NodeTemplate getSaved;
    getSaved.type = "Get_Saved_Variable";
    getSaved.category = "Save";
    getSaved.moduleName = "Stargraph";
    getSaved.createPins = [](Node* node) {
        node->inputs.push_back({"Variable_Name", PinType::String, true, false, 0, ""});
        node->inputs.push_back({"Target File",   PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Value", PinType::Float, false, true});
    };
    getSaved.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        std::string varName = InputValue(node, "Variable_Name");
        Variable* var = graph.FindVariable(varName);
        std::string targetFile = InputValue(node, "Target File");
        std::string file = !targetFile.empty() ? targetFile : (var ? var->saveFile : "");
        if (file.empty()) return;

        auto entries = SaveFileFormat::ReadEntries(file);
        auto it = entries.find(VariableKey(ctx.actor, varName));
        if (it == entries.end()) return;
        for (Pin& p : node.outputs) if (p.name == "Value") { p.defaultValue = it->second; break; }
    };
    NodeRegistry::Get().RegisterNode(getSaved);

    NodeTemplate loadVar;
    loadVar.type = "Load_Saved_Variable";
    loadVar.category = "Save";
    loadVar.moduleName = "Stargraph";
    loadVar.createPins = [](Node* node) {
        node->inputs.push_back({"In",            PinType::Exec,   true,  false});
        node->inputs.push_back({"Variable_Name", PinType::String, true,  false, 0, ""});
        node->inputs.push_back({"Target File",   PinType::String, true,  false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Value", PinType::Float, false, true});
    };
    loadVar.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        std::string varName = InputValue(node, "Variable_Name");
        Variable* var = graph.FindVariable(varName);
        if (!var) return;
        std::string targetFile = InputValue(node, "Target File");
        std::string file = !targetFile.empty() ? targetFile : var->saveFile;
        if (file.empty()) return;

        auto entries = SaveFileFormat::ReadEntries(file);
        auto it = entries.find(VariableKey(ctx.actor, varName));
        if (it == entries.end()) return;
        var->value = it->second;
        for (Pin& p : node.outputs) if (p.name == "Value") { p.defaultValue = it->second; break; }
    };
    NodeRegistry::Get().RegisterNode(loadVar);

    NodeTemplate loadFile;
    loadFile.type = "Load_File";
    loadFile.category = "Save";
    loadFile.moduleName = "Stargraph";
    loadFile.createPins = [](Node* node) {
        node->inputs.push_back({"In",          PinType::Exec,   true, false});
        node->inputs.push_back({"Target File", PinType::String, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    loadFile.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        std::string targetFile = InputValue(node, "Target File");
        if (targetFile.empty()) return;

        auto entries = SaveFileFormat::ReadEntries(targetFile);

        for (Variable& v : graph.GetVariables()) {
            if (v.saveFile != targetFile) continue;
            auto it = entries.find(VariableKey(ctx.actor, v.name));
            if (it != entries.end()) v.value = it->second;
        }
        for (Component* comp : ctx.actor->GetComponents()) {
            PropertyList props;
            comp->GetProperties(props);
            for (const PropertyHandle& h : props.GetAll()) {
                if (h.readOnly) continue;
                auto sit = comp->savableFields.find(h.name);
                if (sit == comp->savableFields.end() || sit->second != targetFile) continue;
                auto eit = entries.find(ComponentFieldKey(ctx.actor, comp, h.name));
                if (eit != entries.end()) PropertyFromString(h, eit->second);
            }
        }
    };
    NodeRegistry::Get().RegisterNode(loadFile);

    NodeTemplate loadAll;
    loadAll.type = "Load_All";
    loadAll.category = "Save";
    loadAll.moduleName = "Stargraph";
    loadAll.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    loadAll.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> fileCache;
        auto entriesFor = [&](const std::string& file) -> std::unordered_map<std::string, std::string>& {
            auto it = fileCache.find(file);
            if (it != fileCache.end()) return it->second;
            return fileCache[file] = SaveFileFormat::ReadEntries(file);
        };

        for (Variable& v : graph.GetVariables()) {
            if (v.saveFile.empty()) continue;
            auto& entries = entriesFor(v.saveFile);
            auto it = entries.find(VariableKey(ctx.actor, v.name));
            if (it != entries.end()) v.value = it->second;
        }
        for (Component* comp : ctx.actor->GetComponents()) {
            PropertyList props;
            comp->GetProperties(props);
            for (const PropertyHandle& h : props.GetAll()) {
                if (h.readOnly) continue;
                auto sit = comp->savableFields.find(h.name);
                if (sit == comp->savableFields.end() || sit->second.empty()) continue;
                auto& entries = entriesFor(sit->second);
                auto eit = entries.find(ComponentFieldKey(ctx.actor, comp, h.name));
                if (eit != entries.end()) PropertyFromString(h, eit->second);
            }
        }
    };
    NodeRegistry::Get().RegisterNode(loadAll);
}
