#pragma once
#include <string>
#include <vector>
#include <deque>
#include <cstdlib>
#include <cstdio>
#include <ostream>

enum class PinType {
    None,
    Exec,
    Boolean,
    Integer,
    Float,
    String,
    Vec3,
    Object,
    Wildcard
};

enum class ReplicationMode {
    Local,          // Never leaves the machine.
    Replicated,     // Client Authoritative. Writes sync up to host, then from host to everyone.
    ServerOnly      // Write-Gated to Host, syncs out to clients. Clients can't write and only read this.
};

std::string ReplicationModeToString(ReplicationMode mode);
ReplicationMode ReplicationModeFromString(const std::string& text);

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
};

struct Link {
    int outputPinId = 0;
    int inputPinId = 0;
};

struct Variable {
    std::string name;
    PinType type = PinType::None;
    std::string value;
    bool isArray = false;
    std::vector<std::string> arrayValues;

    ReplicationMode replication = ReplicationMode::Local;

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

    // MeshComponent
    std::string meshAssetPath;
    std::vector<std::string> materialAssetPaths;

    // CollisionComponent
    std::string collisionShape = "Box";
    float collisionRadius = 50.0f;
    float collisionHeight = 100.0f;

    // AudioComponent
    std::string soundAssetPath;
    bool autoPlay = false;
    bool looping = false;
    float volume = 1.0f;

    // CameraComponent
    float cameraFov = 50.0f;
    bool cameraOrthographic = false;
};

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

    // Custom Event stuff
    Pin* AddDynamicPin(Node& node, bool isOutput, const std::string& name, PinType type);
    void RemoveDynamicPin(Node& node, bool isOutput, const std::string& pinName);
    int AllocatePinId() { return nextPinId++; }

    Variable* FindVariable(const std::string& name);
    const Variable* FindVariable(const std::string& name) const;

    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path);

    std::string assetName;
    std::string parentClass = "Actor";
    int formatVersion = 1;

private:
    std::deque<Node> nodes;
    std::vector<Link> links;
    std::vector<Variable> variables;
    std::vector<ComponentDecl> components;
    int nextPinId = 1;
    int nextNodeId = 1;
};

std::string PinTypeToString(PinType type);
PinType PinTypeFromString(const std::string& text);