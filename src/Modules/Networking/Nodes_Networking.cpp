#include "Nodes_Networking.h"
#include "NetworkingModule.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include <cstdlib>

void RegisterNetworkingNodes() {
    NodeTemplate hostNode;
    hostNode.type = "Host";
    hostNode.category = "Networking";
    hostNode.moduleName = "Networking";
    hostNode.createPins = [](Node* node) {
        node->inputs.push_back({"In",          PinType::Exec,    true,  false});
        node->inputs.push_back({"Port",        PinType::Integer, true,  false, 0, "7777"});
        node->inputs.push_back({"Password",    PinType::String,  true,  false, 0, ""});
        node->inputs.push_back({"Max Players", PinType::Integer, true,  false, 0, "8"});
        node->inputs.push_back({"Turn Server", PinType::String,  true,  false, 0, ""});
        node->outputs.push_back({"Out",     PinType::Exec,    false, true});
        node->outputs.push_back({"Success", PinType::Boolean, false, true});
    };
    hostNode.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        int port = 7777;
        std::string password, turnServer;
        int maxPlayers = 8;
        for (const Pin& p : node.inputs) {
            if (p.name == "Port") port = atoi(p.defaultValue.c_str());
            else if (p.name == "Password") password = p.defaultValue;
            else if (p.name == "Max Players") maxPlayers = atoi(p.defaultValue.c_str());
            else if (p.name == "Turn Server") turnServer = p.defaultValue;
        }

        bool success = false;
        if (NetworkingModule* mod = NetworkingModule::Instance()) {
            std::string error = mod->GetBackend().StartHost((uint16_t)port, password, maxPlayers, turnServer);
            success = error.empty();
            if (success) mod->BeginSession();
        }
        for (Pin& p : node.outputs) if (p.name == "Success") p.defaultValue = success ? "1" : "0";
    };
    NodeRegistry::Get().RegisterNode(hostNode);

    NodeTemplate joinNode;
    joinNode.type = "Join";
    joinNode.category = "Networking";
    joinNode.moduleName = "Networking";
    joinNode.createPins = [](Node* node) {
        node->inputs.push_back({"In",          PinType::Exec,   true,  false});
        node->inputs.push_back({"Address",     PinType::String, true,  false, 0, "127.0.0.1:7777"});
        node->inputs.push_back({"Password",    PinType::String, true,  false, 0, ""});
        node->inputs.push_back({"Turn Server", PinType::String, true,  false, 0, ""});
        node->outputs.push_back({"Out",     PinType::Exec,    false, true});
        node->outputs.push_back({"Success", PinType::Boolean, false, true});
    };
    joinNode.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string address = "127.0.0.1:7777", password, turnServer;
        for (const Pin& p : node.inputs) {
            if (p.name == "Address") address = p.defaultValue;
            else if (p.name == "Password") password = p.defaultValue;
            else if (p.name == "Turn Server") turnServer = p.defaultValue;
        }

        bool success = false;
        if (NetworkingModule* mod = NetworkingModule::Instance()) {
            std::string error = mod->GetBackend().StartJoin(address, password, turnServer);
            success = error.empty();
            
            if (success) mod->BeginSession();
        }
        for (Pin& p : node.outputs) if (p.name == "Success") p.defaultValue = success ? "1" : "0";
    };
    NodeRegistry::Get().RegisterNode(joinNode);

    NodeTemplate disconnectNode;
    disconnectNode.type = "Disconnect";
    disconnectNode.category = "Networking";
    disconnectNode.moduleName = "Networking";
    disconnectNode.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    disconnectNode.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (NetworkingModule* mod = NetworkingModule::Instance()) {
            mod->EndSession();
        }
    };
    NodeRegistry::Get().RegisterNode(disconnectNode);
}
