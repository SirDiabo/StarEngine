#pragma once
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>
#include <ostream>
#include <fstream>
#include "Runtime/ReplicationMode.h"
#include "Core/SectionFileIO.h"

enum class PinType {
    None,
    Exec,
    Boolean,
    Integer,
    Float,
    String,
    Vec3,
    Rotator,
    Object,
    Array,
    Wildcard
};


class PinValue {
public:
    struct Vec3Value { float x = 0.0f, y = 0.0f, z = 0.0f; };

    PinValue() = default;
    PinValue(const char* s) : str(s) {}
    PinValue(std::string s) : str(std::move(s)) {}

    PinValue& operator=(const char* s) { str = s; Invalidate(); return *this; }
    PinValue& operator=(const std::string& s) { str = s; Invalidate(); return *this; }

    operator const std::string&() const { return str; }
    const char* c_str() const { return str.c_str(); }
    bool empty() const { return str.empty(); }
    size_t size() const { return str.size(); }

    bool operator==(const std::string& o) const { return str == o; }
    bool operator==(const char* o) const { return str == o; }
    bool operator!=(const std::string& o) const { return str != o; }
    bool operator!=(const char* o) const { return str != o; }

    float AsFloat() const {
        if (!floatCacheValid) { cachedFloat = (float)atof(str.c_str()); floatCacheValid = true; }
        return cachedFloat;
    }
    int AsInt() const {
        if (!intCacheValid) { cachedInt = atoi(str.c_str()); intCacheValid = true; }
        return cachedInt;
    }
    bool AsBool() const { return str == "true" || str == "1"; }
    Vec3Value AsVec3() const {
        if (!vecCacheValid) {
            cachedVec = Vec3Value{};
            sscanf(str.c_str(), "%f,%f,%f", &cachedVec.x, &cachedVec.y, &cachedVec.z);
            vecCacheValid = true;
        }
        return cachedVec;
    }

    void SetFloat(float v, const char* format = "%.2f") {
        char buf[32];
        snprintf(buf, sizeof(buf), format, v);
        str = buf;
        Invalidate();
        cachedFloat = v;
        floatCacheValid = true;
    }
    void SetInt(int v) {
        str = std::to_string(v);
        Invalidate();
        cachedInt = v;
        intCacheValid = true;
    }
    void SetBool(bool v) {
        str = v ? "true" : "false";
        Invalidate();
    }
    void SetVec3(float x, float y, float z) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f", x, y, z);
        str = buf;
        Invalidate();
        cachedVec = Vec3Value{x, y, z};
        vecCacheValid = true;
    }

private:
    std::string str;
    mutable bool floatCacheValid = false;
    mutable float cachedFloat = 0.0f;
    mutable bool intCacheValid = false;
    mutable int cachedInt = 0;
    mutable bool vecCacheValid = false;
    mutable Vec3Value cachedVec;

    void Invalidate() { floatCacheValid = intCacheValid = vecCacheValid = false; }
};

struct Pin {
    std::string name;
    PinType type = PinType::None;
    bool isInput = false;
    bool isOutput = false;
    int id = 0;
    PinValue defaultValue;
    bool isWildcard = false;
    std::string objectKind;
    PinType elementType = PinType::None;
};

inline std::string operator+(const std::string& lhs, const PinValue& rhs) { return lhs + (const std::string&)rhs; }
inline std::string operator+(const PinValue& lhs, const std::string& rhs) { return (const std::string&)lhs + rhs; }
inline std::string operator+(const char* lhs, const PinValue& rhs) { return lhs + (const std::string&)rhs; }
inline std::string operator+(const PinValue& lhs, const char* rhs) { return (const std::string&)lhs + rhs; }

inline std::ostream& operator<<(std::ostream& os, const PinValue& v) { return os << (const std::string&)v; }

inline void ReadVec3(const Pin& p, float& x, float& y, float& z) {
    PinValue::Vec3Value v = p.defaultValue.AsVec3();
    x = v.x; y = v.y; z = v.z;
}

inline bool ReadBool(const Pin& p) {
    return p.defaultValue.AsBool();
}

inline float ReadFloat(const Pin& p) {
    return p.defaultValue.AsFloat();
}

struct Node {
    std::string id;
    std::string type;
    std::string title;
    float posX = 0, posY = 0;
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;
    std::string componentClass;

    ReplicationMode eventReplication = ReplicationMode::Local;
    bool eventReliable = false;

    std::vector<std::string> execCases;

    // Runtime-only flow control state
    bool flowState = false;

    // Runtime-only held-input edge state
    bool heldPrevState = false;

    // Runtime-only wildcard resolution cache
    bool wildcardResolved = false;
};

struct Link {
    int outputPinId = 0;
    int inputPinId = 0;
    std::string id;
};

struct Variable {
    std::string name;
    PinType type = PinType::None;
    std::string value;
    std::string defaultValue;
    std::string objectKind;
    bool isArray = false;
    std::vector<std::string> arrayValues;

    ReplicationMode replication = ReplicationMode::Local;
    std::string saveFile; // Name of a project Save File (SaveFileRegistry). Empty = not saved.

    std::string DefaultElementText() const;
};

// Design-time declaration of a component owned by a StarActor class (.staractor asset).
struct ComponentDecl {
    std::string componentType = "SceneComponent";
    std::string name;

    // Name of another component on the same actor this one is parented to.
    std::string parentName;

    // Transform relative to the parent named above (or the actor's root component if empty).
    float posX = 0, posY = 0, posZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0;
    float scaleX = 1, scaleY = 1, scaleZ = 1;

    std::unordered_map<std::string, std::string> fields;
    std::unordered_map<std::string, std::vector<std::string>> arrayFields;

    std::string Get(const std::string& key, const std::string& def = "") const {
        auto it = fields.find(key);
        return it != fields.end() ? it->second : def;
    }

    // SaveFileRegistry
    std::unordered_map<std::string, std::string> savableFields;
};

struct FunctionDef;

class Stargraph {
public:
    const std::deque<Node>& GetNodes() const { return nodes; }
    const std::vector<Link>& GetLinks() const { return links; }
    const std::vector<Variable>& GetVariables() const { return variables; }
    std::vector<Variable>& GetVariables() { return variables; }
    const std::vector<ComponentDecl>& GetComponents() const { return components; }
    std::vector<ComponentDecl>& GetComponents() { return components; }

    Node* CreateNode(const std::string& type);
    void AddLink(const Link& link);
    void RemoveLink(int pinId);
    void RemoveSpecificLink(int outputPinId, int inputPinId);
    void AddVariable(const Variable& var);
    void RemoveVariable(size_t index);
    void RenameVariable(size_t index, const std::string& newName);
    void AddComponent(const ComponentDecl& comp);
    void RemoveComponent(size_t index);

    Pin* FindPin(int pinId);
    Node* FindNodeByPinId(int pinId);
    Node* FindNodeByType(const std::string& type);
    Node* FindCustomEventByName(const std::string& eventName);
    Node* FindNodeById(int id);
    bool RemoveNode(int nodeId);

    // Nodes tagged by type, built once and reused until a node is created or removed
    const std::vector<Node*>& GetNodesOfType(const std::string& type);

    // Custom Event stuff
    Pin* AddDynamicPin(Node& node, bool isOutput, const std::string& name, PinType type);
    void RemoveDynamicPin(Node& node, bool isOutput, const std::string& pinName);
    int AllocatePinId() { return nextPinId++; }

    void SyncArrayVariables();
    Pin* AddDynamicWildcardInput(Node& node);
    bool RemoveDynamicWildcardInput(Node& node, int pinId);
    void RebuildDynamicExecOutputs(Node& node);
    bool AddExecCase(Node& node, const std::string& input);
    bool RemoveExecCase(Node& node, int caseIndex);
    bool SetExecCase(Node& node, int caseIndex, const std::string& input);

    Variable* FindVariable(const std::string& name);
    const Variable* FindVariable(const std::string& name) const;
    void CaptureVariableDefaults();

    const std::vector<FunctionDef>& GetFunctions() const { return functions; }
    std::vector<FunctionDef>& GetFunctions() { return functions; }
    FunctionDef* AddFunction(const std::string& name);
    void RemoveFunction(size_t index);
    void RenameFunction(size_t index, const std::string& newName);
    FunctionDef* FindFunction(const std::string& name);
    const FunctionDef* FindFunction(const std::string& name) const;

    const std::vector<Link*>& GetLinksFromPin(int outputPinId);
    Link* GetLinkToPin(int inputPinId);

    PinType GetConnectedOutputType(int inputPinId);

    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path);
    void SaveGraphContent(std::ostream& out, const std::string& prefix) const;
    bool LoadGraphContent(const std::vector<SectionFileIO::ParsedSection>& sections, const std::string& prefix);

    std::string assetName;
    std::string parentClass = "Actor";
    int formatVersion = 1;
    int customEventDepth = 0;

private:
    std::deque<Node> nodes;
    std::vector<Link> links;
    std::vector<Variable> variables;
    std::vector<ComponentDecl> components;
    std::vector<FunctionDef> functions;
    int nextPinId = 1;
    int nextNodeId = 1;
    int nextLinkId = 1;

    std::unordered_map<std::string, std::vector<Node*>> nodesByTypeCache;
    std::unordered_map<int, Node*> pinToNodeCache;
    std::unordered_map<int, std::vector<Link*>> outputPinToLinksCache;
    std::unordered_map<int, Link*> inputPinToLinkCache;
    std::unordered_map<std::string, Variable*> variableByNameCache;
    bool cachesBuilt = false;
    void InvalidateCaches();
    void BuildCaches();
};

std::string PinTypeToString(PinType type);
PinType PinTypeFromString(const std::string& text);

struct FunctionDef {
    std::string name;
    Stargraph graph;
};
