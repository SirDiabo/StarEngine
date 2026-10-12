#include "Nodes_Array.h"
#include "NodeRegistry.h"
#include "GraphExecutor.h"
#include "StargraphArray.h"
#include "Nodes_Variables.h"
#include "Runtime/Networking/NetworkBackend.h"
#include "raylib.h"
#include <string>
#include <vector>

namespace {

Pin MakeArrayPin(const char* name, bool isInput) {
    Pin pin;
    pin.name = name;
    pin.type = PinType::Array;
    pin.isInput = isInput;
    pin.isOutput = !isInput;
    pin.elementType = PinType::Wildcard;
    return pin;
}

Pin MakeItemPin(const char* name, bool isInput) {
    Pin pin;
    pin.name = name;
    pin.type = PinType::Wildcard;
    pin.isInput = isInput;
    pin.isOutput = !isInput;
    return pin;
}

Pin MakeIntPin(const char* name, bool isInput, const char* defaultValue) {
    Pin pin;
    pin.name = name;
    pin.type = PinType::Integer;
    pin.isInput = isInput;
    pin.isOutput = !isInput;
    pin.defaultValue = defaultValue;
    return pin;
}

Pin MakeBoolPin(const char* name) {
    Pin pin;
    pin.name = name;
    pin.type = PinType::Boolean;
    pin.isInput = false;
    pin.isOutput = true;
    return pin;
}

Pin MakeExecPin(const char* name, bool isInput) {
    Pin pin;
    pin.name = name;
    pin.type = PinType::Exec;
    pin.isInput = isInput;
    pin.isOutput = !isInput;
    return pin;
}

std::string InputText(const Node& node, const char* name) {
    for (const Pin& p : node.inputs) if (p.name == name) return p.defaultValue;
    return "";
}

void SetOutput(Node& node, const char* name, const std::string& value) {
    for (Pin& p : node.outputs) if (p.name == name) { p.defaultValue = value; return; }
}

bool IsConcrete(PinType type) {
    return type != PinType::None && type != PinType::Wildcard && type != PinType::Array && type != PinType::Exec;
}

PinType SourceElementType(Stargraph& graph, const Pin& inputPin) {
    Link* link = graph.GetLinkToPin(inputPin.id);
    if (!link) return PinType::None;
    Node* from = graph.FindNodeByPinId(link->outputPinId);
    if (!from) return PinType::None;
    for (const Pin& p : from->outputs) {
        if (p.id != link->outputPinId) continue;
        if (p.type == PinType::Array) return IsConcrete(p.elementType) ? p.elementType : PinType::None;
        return IsConcrete(p.type) ? p.type : PinType::None;
    }
    return PinType::None;
}

PinType ResolveElementType(Stargraph& graph, Node& node) {
    for (const Pin& p : node.inputs) {
        if (p.type != PinType::Array) continue;
        if (IsConcrete(p.elementType)) return p.elementType;
        PinType fromLink = SourceElementType(graph, p);
        if (fromLink != PinType::None) return fromLink;
    }
    for (const Pin& p : node.inputs) {
        if (p.name.rfind("Item", 0) != 0) continue;
        if (IsConcrete(p.type)) return p.type;
        PinType fromLink = SourceElementType(graph, p);
        if (fromLink != PinType::None) return fromLink;
    }
    for (const Pin& p : node.outputs) {
        if (p.name == "Item" && IsConcrete(p.type)) return p.type;
    }
    return PinType::String;
}

std::vector<std::string> ReadArray(const Node& node) {
    return StarArray::Decode(InputText(node, "Array"));
}

std::string ReadItem(Stargraph& graph, Node& node) {
    std::string item = InputText(node, "Item");
    if (item.empty()) item = StarArray::DefaultElementText(ResolveElementType(graph, node));
    return item;
}

int ReadIndex(const Node& node) {
    std::string text = InputText(node, "Index");
    return text.empty() ? 0 : std::atoi(text.c_str());
}

Variable* FindSourceVariable(Stargraph& graph, Node& node, ExecutionContext& ctx) {
    Node* current = &node;
    for (int hop = 0; hop < 32 && current; hop++) {
        const Pin* arrayInput = nullptr;
        for (const Pin& pin : current->inputs) if (pin.name == "Array") { arrayInput = &pin; break; }
        if (!arrayInput) return nullptr;

        Link* link = graph.GetLinkToPin(arrayInput->id);
        if (!link) return nullptr;
        Node* source = graph.FindNodeByPinId(link->outputPinId);
        if (!source) return nullptr;

        if (source->type == "Get_Variable") return ResolveVariableForNode(*source, graph, ctx);

        bool passesArrayThrough = source->type.rfind("Array_", 0) == 0;
        if (!passesArrayThrough) return nullptr;
        current = source;
    }
    return nullptr;
}

void CommitArray(Stargraph& graph, Node& node, ExecutionContext& ctx, const std::vector<std::string>& items) {
    std::string encoded = StarArray::Encode(items);
    SetOutput(node, "Array", encoded);

    Variable* variable = FindSourceVariable(graph, node, ctx);
    if (!variable || !variable->isArray) return;
    if (variable->replication == ReplicationMode::ServerOnly && !NetworkBackendRegistry::Get().HasAuthority()) return;
    variable->value = encoded;
}

void RegisterPure(const char* type, const char* label,
                  std::function<void(Node*)> createPins,
                  std::function<void(Node&, Stargraph&, ExecutionContext&)> execute) {
    NodeTemplate t;
    t.type = type;
    t.menuLabel = label;
    t.category = "Arrays";
    t.moduleName = "Stargraph";
    t.createPins = std::move(createPins);
    t.execute = std::move(execute);
    NodeRegistry::Get().RegisterNode(t);
}

void RegisterMutator(const char* type, const char* label,
                     std::function<void(Node*)> extraInputs,
                     std::function<void(Node*)> extraOutputs,
                     std::function<void(Node&, Stargraph&, ExecutionContext&, std::vector<std::string>&)> mutate) {
    NodeTemplate t;
    t.type = type;
    t.menuLabel = label;
    t.category = "Arrays";
    t.moduleName = "Stargraph";
    t.createPins = [extraInputs, extraOutputs](Node* node) {
        node->inputs.push_back(MakeExecPin("In", true));
        node->inputs.push_back(MakeArrayPin("Array", true));
        if (extraInputs) extraInputs(node);
        node->outputs.push_back(MakeExecPin("Out", false));
        node->outputs.push_back(MakeArrayPin("Array", false));
        if (extraOutputs) extraOutputs(node);
    };
    t.execute = [mutate](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::vector<std::string> items = ReadArray(node);
        mutate(node, graph, ctx, items);
        CommitArray(graph, node, ctx, items);
    };
    NodeRegistry::Get().RegisterNode(t);
}

}

void RegisterArrayNodes() {
    {
        NodeTemplate t;
        t.type = "Make_Array";
        t.menuLabel = "Make Array";
        t.category = "Arrays";
        t.moduleName = "Stargraph";
        t.dynamicWildcardInputPrefix = "Item ";
        t.dynamicWildcardInputInitial = 2;
        t.createPins = [](Node* node) {
            node->outputs.push_back(MakeArrayPin("Array", false));
        };
        t.execute = [](Node& node, Stargraph& graph, ExecutionContext&) {
            PinType elementType = ResolveElementType(graph, node);
            std::vector<std::string> items;
            for (const Pin& p : node.inputs) {
                if (p.name.rfind("Item ", 0) != 0) continue;
                std::string value = p.defaultValue;
                if (value.empty()) value = StarArray::DefaultElementText(elementType);
                items.push_back(value);
            }
            SetOutput(node, "Array", StarArray::Encode(items));
        };
        NodeRegistry::Get().RegisterNode(t);
    }

    RegisterPure("Array_Length", "Array Length",
        [](Node* node) {
            node->inputs.push_back(MakeArrayPin("Array", true));
            node->outputs.push_back(MakeIntPin("Length", false, "0"));
        },
        [](Node& node, Stargraph&, ExecutionContext&) {
            SetOutput(node, "Length", std::to_string(ReadArray(node).size()));
        });

    RegisterPure("Array_Get", "Array Get",
        [](Node* node) {
            node->inputs.push_back(MakeArrayPin("Array", true));
            node->inputs.push_back(MakeIntPin("Index", true, "0"));
            node->outputs.push_back(MakeItemPin("Item", false));
        },
        [](Node& node, Stargraph& graph, ExecutionContext&) {
            std::vector<std::string> items = ReadArray(node);
            int index = ReadIndex(node);
            if (index >= 0 && index < (int)items.size()) {
                SetOutput(node, "Item", items[index]);
            } else {
                TraceLog(LOG_WARNING, "Array Get: index %d is out of range (length %d)", index, (int)items.size());
                SetOutput(node, "Item", StarArray::DefaultElementText(ResolveElementType(graph, node)));
            }
        });

    RegisterPure("Array_Is_Valid_Index", "Array Is Valid Index",
        [](Node* node) {
            node->inputs.push_back(MakeArrayPin("Array", true));
            node->inputs.push_back(MakeIntPin("Index", true, "0"));
            node->outputs.push_back(MakeBoolPin("Valid"));
        },
        [](Node& node, Stargraph&, ExecutionContext&) {
            int index = ReadIndex(node);
            SetOutput(node, "Valid", (index >= 0 && index < (int)ReadArray(node).size()) ? "true" : "false");
        });

    RegisterPure("Array_Contains", "Array Contains",
        [](Node* node) {
            node->inputs.push_back(MakeArrayPin("Array", true));
            node->inputs.push_back(MakeItemPin("Item", true));
            node->outputs.push_back(MakeBoolPin("Contains"));
        },
        [](Node& node, Stargraph& graph, ExecutionContext&) {
            PinType type = ResolveElementType(graph, node);
            int found = StarArray::IndexOf(type, ReadArray(node), ReadItem(graph, node));
            SetOutput(node, "Contains", found >= 0 ? "true" : "false");
        });

    RegisterPure("Array_Find", "Array Find",
        [](Node* node) {
            node->inputs.push_back(MakeArrayPin("Array", true));
            node->inputs.push_back(MakeItemPin("Item", true));
            node->outputs.push_back(MakeIntPin("Index", false, "-1"));
        },
        [](Node& node, Stargraph& graph, ExecutionContext&) {
            PinType type = ResolveElementType(graph, node);
            SetOutput(node, "Index", std::to_string(StarArray::IndexOf(type, ReadArray(node), ReadItem(graph, node))));
        });

    RegisterMutator("Array_Add", "Array Add",
        [](Node* node) { node->inputs.push_back(MakeItemPin("Item", true)); },
        [](Node* node) { node->outputs.push_back(MakeIntPin("Index", false, "0")); },
        [](Node& node, Stargraph& graph, ExecutionContext&, std::vector<std::string>& items) {
            items.push_back(ReadItem(graph, node));
            SetOutput(node, "Index", std::to_string(items.size() - 1));
        });

    RegisterMutator("Array_Add_Unique", "Array Add Unique",
        [](Node* node) { node->inputs.push_back(MakeItemPin("Item", true)); },
        [](Node* node) { node->outputs.push_back(MakeBoolPin("Added")); },
        [](Node& node, Stargraph& graph, ExecutionContext&, std::vector<std::string>& items) {
            PinType type = ResolveElementType(graph, node);
            std::string item = ReadItem(graph, node);
            bool exists = StarArray::IndexOf(type, items, item) >= 0;
            if (!exists) items.push_back(item);
            SetOutput(node, "Added", exists ? "false" : "true");
        });

    RegisterMutator("Array_Insert", "Array Insert",
        [](Node* node) {
            node->inputs.push_back(MakeIntPin("Index", true, "0"));
            node->inputs.push_back(MakeItemPin("Item", true));
        },
        nullptr,
        [](Node& node, Stargraph& graph, ExecutionContext&, std::vector<std::string>& items) {
            int index = ReadIndex(node);
            if (index < 0) index = 0;
            if (index > (int)items.size()) index = (int)items.size();
            items.insert(items.begin() + index, ReadItem(graph, node));
        });

    RegisterMutator("Array_Set", "Array Set",
        [](Node* node) {
            node->inputs.push_back(MakeIntPin("Index", true, "0"));
            node->inputs.push_back(MakeItemPin("Item", true));
        },
        nullptr,
        [](Node& node, Stargraph& graph, ExecutionContext&, std::vector<std::string>& items) {
            int index = ReadIndex(node);
            if (index < 0 || index >= (int)items.size()) {
                TraceLog(LOG_WARNING, "Array Set: index %d is out of range (length %d)", index, (int)items.size());
                return;
            }
            items[index] = ReadItem(graph, node);
        });

    RegisterMutator("Array_Remove_Index", "Array Remove Index",
        [](Node* node) { node->inputs.push_back(MakeIntPin("Index", true, "0")); },
        [](Node* node) { node->outputs.push_back(MakeBoolPin("Removed")); },
        [](Node& node, Stargraph&, ExecutionContext&, std::vector<std::string>& items) {
            int index = ReadIndex(node);
            bool valid = index >= 0 && index < (int)items.size();
            if (valid) items.erase(items.begin() + index);
            SetOutput(node, "Removed", valid ? "true" : "false");
        });

    RegisterMutator("Array_Remove_Item", "Array Remove Item",
        [](Node* node) { node->inputs.push_back(MakeItemPin("Item", true)); },
        [](Node* node) { node->outputs.push_back(MakeBoolPin("Removed")); },
        [](Node& node, Stargraph& graph, ExecutionContext&, std::vector<std::string>& items) {
            PinType type = ResolveElementType(graph, node);
            int found = StarArray::IndexOf(type, items, ReadItem(graph, node));
            if (found >= 0) items.erase(items.begin() + found);
            SetOutput(node, "Removed", found >= 0 ? "true" : "false");
        });

    RegisterMutator("Array_Clear", "Array Clear",
        nullptr,
        nullptr,
        [](Node&, Stargraph&, ExecutionContext&, std::vector<std::string>& items) {
            items.clear();
        });

    {
        NodeTemplate t;
        t.type = "For_Each_Loop";
        t.menuLabel = "For Each Loop";
        t.category = "Arrays";
        t.moduleName = "Stargraph";
        t.createPins = [](Node* node) {
            node->inputs.push_back(MakeExecPin("In", true));
            node->inputs.push_back(MakeArrayPin("Array", true));
            node->outputs.push_back(MakeExecPin("Loop Body", false));
            node->outputs.push_back(MakeItemPin("Item", false));
            node->outputs.push_back(MakeIntPin("Index", false, "0"));
            node->outputs.push_back(MakeExecPin("Completed", false));
        };
        t.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
            std::vector<std::string> items = ReadArray(node);
            for (size_t i = 0; i < items.size(); i++) {
                SetOutput(node, "Item", items[i]);
                SetOutput(node, "Index", std::to_string(i));
                GraphExecutor::RunFromPin(graph, &node, ctx, "Loop Body");
            }
            GraphExecutor::RunFromPin(graph, &node, ctx, "Completed");
        };
        NodeRegistry::Get().RegisterNode(t);
    }
}
