#include "Nodes_Actor.h"
#include "NodeRegistry.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "StargraphBehavior.h"
#include "Core/CookedFile.h"
#include "raymath.h"
#include <cstdlib>
#include <fstream>

void RegisterActorNodes() {
    NodeTemplate getActorLocation;
    getActorLocation.type = "Get_Actor_Location";
    getActorLocation.category = "Actor";
    getActorLocation.moduleName = "Stargraph";
    getActorLocation.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Location", PinType::Vec3, false, true});
    };
    getActorLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f", target->position.x, target->position.y, target->position.z);
        for (Pin& p : node.outputs) if (p.name == "Location") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorLocation);

    NodeTemplate setActorLocation;
    setActorLocation.type = "Set_Actor_Location";
    setActorLocation.category = "Actor";
    setActorLocation.moduleName = "Stargraph";
    setActorLocation.createPins = [](Node* node) {
        node->inputs.push_back({"In",       PinType::Exec, true,  false});
        node->inputs.push_back({"Target",   PinType::Object, true,  false, 0, ""});
        node->inputs.push_back({"Location", PinType::Vec3, true,  false, 0, "0,0,0"});
        node->outputs.push_back({"Out",     PinType::Exec, false, true});
    };
    setActorLocation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Location") {
                sscanf(p.defaultValue.c_str(), "%f,%f,%f",
                    &target->position.x, &target->position.y, &target->position.z);
                break;
            }
        }
    };
    NodeRegistry::Get().RegisterNode(setActorLocation);

    NodeTemplate getActorRotation;
    getActorRotation.type = "Get_Actor_Rotation";
    getActorRotation.category = "Actor";
    getActorRotation.moduleName = "Stargraph";
    getActorRotation.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Rotation", PinType::Vec3, false, true});
    };
    getActorRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f",
            target->rotation.x, target->rotation.y, target->rotation.z);
        for (Pin& p : node.outputs) if (p.name == "Rotation") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorRotation);

    NodeTemplate setActorRotation;
    setActorRotation.type = "Set_Actor_Rotation";
    setActorRotation.category = "Actor";
    setActorRotation.moduleName = "Stargraph";
    setActorRotation.createPins = [](Node* node) {
        node->inputs.push_back({"In",       PinType::Exec, true,  false});
        node->inputs.push_back({"Target",   PinType::Object, true,  false, 0, ""});
        node->inputs.push_back({"Rotation", PinType::Vec3, true,  false, 0, "0,0,0"});
        node->outputs.push_back({"Out",     PinType::Exec, false, true});
    };
    setActorRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Rotation") {
                sscanf(p.defaultValue.c_str(), "%f,%f,%f",
                    &target->rotation.x, &target->rotation.y, &target->rotation.z);
                break;
            }
        }
    };
    NodeRegistry::Get().RegisterNode(setActorRotation);

    NodeTemplate getActorScale;
    getActorScale.type = "Get_Actor_Scale";
    getActorScale.category = "Actor";
    getActorScale.moduleName = "Stargraph";
    getActorScale.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Scale", PinType::Vec3, false, true});
    };
    getActorScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f",
            target->scale.x, target->scale.y, target->scale.z);
        for (Pin& p : node.outputs) if (p.name == "Scale") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorScale);

    NodeTemplate setActorScale;
    setActorScale.type = "Set_Actor_Scale";
    setActorScale.category = "Actor";
    setActorScale.moduleName = "Stargraph";
    setActorScale.createPins = [](Node* node) {
        node->inputs.push_back({"In",    PinType::Exec, true,  false});
        node->inputs.push_back({"Target", PinType::Object, true,  false, 0, ""});
        node->inputs.push_back({"Scale", PinType::Vec3, true,  false, 0, "1,1,1"});
        node->outputs.push_back({"Out",  PinType::Exec, false, true});
    };
    setActorScale.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        for (const Pin& p : node.inputs) {
            if (p.name == "Scale") {
                sscanf(p.defaultValue.c_str(), "%f,%f,%f",
                    &target->scale.x, &target->scale.y, &target->scale.z);
                break;
            }
        }
    };
    NodeRegistry::Get().RegisterNode(setActorScale);

    NodeTemplate getActorForward;
    getActorForward.type = "Get_Actor_Forward_Vector";
    getActorForward.category = "Actor";
    getActorForward.moduleName = "Stargraph";
    getActorForward.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Forward", PinType::Vec3, false, true});
    };
    getActorForward.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        float yr = target->rotation.y * DEG2RAD;
        float pr = target->rotation.x * DEG2RAD;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f",
            sinf(yr) * cosf(pr),
            -sinf(pr),
            cosf(yr) * cosf(pr));
        for (Pin& p : node.outputs) if (p.name == "Forward") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorForward);

    NodeTemplate getActorRight;
    getActorRight.type = "Get_Actor_Right_Vector";
    getActorRight.category = "Actor";
    getActorRight.moduleName = "Stargraph";
    getActorRight.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Right", PinType::Vec3, false, true});
    };
    getActorRight.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        float yr = target->rotation.y * DEG2RAD;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f",
            -cosf(yr),
            0.0f,
            sinf(yr));
        for (Pin& p : node.outputs) if (p.name == "Right") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorRight);

    NodeTemplate getActorUp;
    getActorUp.type = "Get_Actor_Up_Vector";
    getActorUp.category = "Actor";
    getActorUp.moduleName = "Stargraph";
    getActorUp.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Up", PinType::Vec3, false, true});
    };
    getActorUp.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        float yr = target->rotation.y * DEG2RAD;
        float pr = target->rotation.x * DEG2RAD;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f",
            sinf(yr) * sinf(pr),
            cosf(pr),
            cosf(yr) * sinf(pr));
        for (Pin& p : node.outputs) if (p.name == "Up") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getActorUp);

    NodeTemplate destroyActor;
    destroyActor.type = "Destroy_Actor";
    destroyActor.category = "Actor";
    destroyActor.moduleName = "Stargraph";
    destroyActor.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
    };
    destroyActor.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (target) World::Get().DestroyActor(target);
    };
    NodeRegistry::Get().RegisterNode(destroyActor);

    NodeTemplate spawnActor;
    spawnActor.type = "Spawn_Actor";
    spawnActor.category = "Actor";
    spawnActor.moduleName = "Stargraph";
    spawnActor.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Class Name", PinType::String, true, false, 0, "Actor"});
        node->inputs.push_back({"Graph Path", PinType::String, false, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
        node->outputs.push_back({"Spawned Actor", PinType::Object, false, true});
    };
    spawnActor.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        std::string className = "Actor";
        std::string graphPath;
        for (const Pin& p : node.inputs) {
            if (p.name == "Class Name") { className = p.defaultValue; break; }
            if (p.name == "Graph Path") { graphPath = p.defaultValue; }
        }
        Actor* spawned = World::Get().SpawnActor(className);
        if (spawned) {
            spawned->SetName(className + "_Instance");
            if (!graphPath.empty()) {
                Stargraph loadedGraph;
                if (loadedGraph.LoadFromFile(graphPath)) {
                    spawned->SetGraphBehavior(new StargraphBehavior(std::move(loadedGraph)));
                }
            }
            for (Pin& p : node.outputs) {
                if (p.name == "Spawned Actor") {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%p", (void*)spawned);
                    p.defaultValue = buf;
                    break;
                }
            }
        }
    };
    NodeRegistry::Get().RegisterNode(spawnActor);
}