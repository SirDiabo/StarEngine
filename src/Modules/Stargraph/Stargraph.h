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

struct Pin {
    std::string name;
    PinType type = PinType::None;
    bool isInput = false;
    bool isOutput = false;
    int id = 0;
    std::string defaultValue;
};

struct Node {
    std::string id;
    std::string type;
    std::string title;
    float posX = 0, posY = 0;
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;
    std::string componentClass;
};

struct Link {
    int outputPinId = 0;
    int inputPinId = 0;
};

struct Variable {
    std::string name;
    PinType type = PinType::None;
    std::string value;
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
    std::string materialAssetPath;

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
    bool RemoveNode(int nodeId);

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