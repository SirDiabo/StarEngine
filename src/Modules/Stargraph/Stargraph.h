#pragma once
#include <string>
#include <vector>
#include <deque>

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

struct Pin {
    std::string name;
    PinType type = PinType::None;
    bool isInput = false;
    bool isOutput = false;
    int id = 0;
    std::string defaultValue;
    bool isWildcard = false;
};

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

    // Transform relative to the actor's root component.
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