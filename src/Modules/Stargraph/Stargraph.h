#pragma once
#include <string>
#include <vector>
#include <unordered_map>

enum class PinType {
    None,
    Boolean,
    Integer,
    Float,
    String,
    Vec3,
    Object
};

struct Pin {
    std::string name;
    PinType type = PinType::None;
    bool isInput = false;
    bool isOutput = false;
    int id = 0;
};

struct Node {
    std::string id;
    std::string type;
    std::string title;
    float posX = 0, posY = 0;
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;
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

class Stargraph {
public:
    const std::vector<Node>& GetNodes() const { return nodes; }
    const std::vector<Link>& GetLinks() const { return links; }
    const std::vector<Variable>& GetVariables() const { return variables; }

    Node* CreateNode(const std::string& type);
    void AddLink(const Link& link);
    void AddVariable(const Variable& var);

private:
    std::vector<Node> nodes;
    std::vector<Link> links;
    std::vector<Variable> variables;
};