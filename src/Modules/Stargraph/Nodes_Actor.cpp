#include "Nodes_Actor.h"
#include "NodeRegistry.h"
#include "Runtime/Actor.h"
#include "Runtime/World.h"
#include "Runtime/Networking/NetworkBackend.h"
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

            NetworkBackend* backend = NetworkBackendRegistry::Get().GetActive();
            if (backend && backend->IsHost()) {
                spawned->SetNetId(backend->AllocateNetId());
                ReplicatedActorSpawn spawn;
                spawn.netId = spawned->GetNetId();
                spawn.className = className;
                spawn.graphPath = graphPath;
                spawn.posX = spawned->position.x; spawn.posY = spawned->position.y; spawn.posZ = spawned->position.z;
                spawn.rotX = spawned->rotation.x; spawn.rotY = spawned->rotation.y; spawn.rotZ = spawned->rotation.z;
                spawn.scaleX = spawned->scale.x; spawn.scaleY = spawned->scale.y; spawn.scaleZ = spawned->scale.z;
                backend->SendActorSpawn(spawn);
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

    NodeTemplate getShowMouseCursor;
    getShowMouseCursor.type = "Get_Show_Mouse_Cursor";
    getShowMouseCursor.category = "Actor";
    getShowMouseCursor.moduleName = "Stargraph";
    getShowMouseCursor.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Show", PinType::Boolean, false, true});
    };
    getShowMouseCursor.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        if (!ctx.actor) return;
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        for (Pin& p : node.outputs) if (p.name == "Show") {
            p.defaultValue = target->showMouseCursor ? "1" : "0";
            break;
        }
    };
    NodeRegistry::Get().RegisterNode(getShowMouseCursor);

    NodeTemplate setShowMouseCursor;
    setShowMouseCursor.type = "Set_Show_Mouse_Cursor";
    setShowMouseCursor.category = "Actor";
    setShowMouseCursor.moduleName = "Stargraph";
    setShowMouseCursor.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->inputs.push_back({"Show", PinType::Boolean, true, false, 0, "1"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setShowMouseCursor.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
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
            if (p.name == "Show") {
                target->showMouseCursor = (p.defaultValue == "1" || p.defaultValue == "true");
                break;
            }
        }
    };
    NodeRegistry::Get().RegisterNode(setShowMouseCursor);

    NodeTemplate getViewTarget;
    getViewTarget.type = "Get_View_Target";
    getViewTarget.category = "Actor";
    getViewTarget.moduleName = "Stargraph";
    getViewTarget.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Object, false, true});
    };
    getViewTarget.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) {
            target = World::Get().GetViewTarget();
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%p", (void*)target);
        for (Pin& p : node.outputs) if (p.name == "Out") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getViewTarget);

    NodeTemplate getInputFocus;
    getInputFocus.type = "Get_Input_Focus";
    getInputFocus.category = "Actor";
    getInputFocus.moduleName = "Stargraph";
    getInputFocus.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Object, false, true});
    };
    getInputFocus.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) {
            target = World::Get().GetInputFocus();
        }
        if (!target) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%p", (void*)target);
        for (Pin& p : node.outputs) if (p.name == "Out") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getInputFocus);

    NodeTemplate setInputFocus;
    setInputFocus.type = "Set_Input_Focus";
    setInputFocus.category = "Actor";
    setInputFocus.moduleName = "Stargraph";
    setInputFocus.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setInputFocus.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (target) World::Get().SetInputFocus(target);
    };
    NodeRegistry::Get().RegisterNode(setInputFocus);

    NodeTemplate addMovementInput;
    addMovementInput.type = "Add_Movement_Input";
    addMovementInput.category = "Actor";
    addMovementInput.moduleName = "Stargraph";
    addMovementInput.createPins = [](Node* node) {
        node->inputs.push_back({"In",             PinType::Exec, true,  false});
        node->inputs.push_back({"Target",         PinType::Object, true, false, 0, ""});
        node->inputs.push_back({"World Direction", PinType::Vec3, true, false, 0, "0,0,1"});
        node->inputs.push_back({"Scale Value",     PinType::Float, true, false, 0, "1.0"});
        node->outputs.push_back({"Out",           PinType::Exec, false, true});
    };
    addMovementInput.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        Vector3 dir = {0, 0, 1};
        float scaleValue = 1.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
            if (p.name == "World Direction") sscanf(p.defaultValue.c_str(), "%f,%f,%f", &dir.x, &dir.y, &dir.z);
            if (p.name == "Scale Value") scaleValue = (float)atof(p.defaultValue.c_str());
        }
        if (!target) return;
        target->AddMovementInput(dir, scaleValue);
    };
    NodeRegistry::Get().RegisterNode(addMovementInput);

    NodeTemplate getMovementSpeed;
    getMovementSpeed.type = "Get_Movement_Speed";
    getMovementSpeed.category = "Actor";
    getMovementSpeed.moduleName = "Stargraph";
    getMovementSpeed.createPins = [](Node* node) {
        node->inputs.push_back({"Target",         PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Speed",           PinType::Float, true, false, 0, "0.0"});
    };
    getMovementSpeed.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (!target) return;
        char buf[64]; snprintf(buf, sizeof(buf), "%g", target->MovementSpeed);
        for (Pin& p : node.outputs) if (p.name == "Speed") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getMovementSpeed);

    NodeTemplate setMovementSpeed;
    setMovementSpeed.type = "Set_Movement_Speed";
    setMovementSpeed.category = "Actor";
    setMovementSpeed.moduleName = "Stargraph";
    setMovementSpeed.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->inputs.push_back({"New Speed", PinType::Float, true, false, 0, "600.0"});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setMovementSpeed.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        float newSpeed = 600.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "New Speed") newSpeed = (float)atof(p.defaultValue.c_str());
        }
        if (!target) return;
        target->MovementSpeed = newSpeed;
    };
    NodeRegistry::Get().RegisterNode(setMovementSpeed);

    NodeTemplate addControlYawInput;
    addControlYawInput.type = "Add_Control_Yaw_Input";
    addControlYawInput.category = "Actor";
    addControlYawInput.moduleName = "Stargraph";
    addControlYawInput.createPins = [](Node* node) {
        node->inputs.push_back({"In",    PinType::Exec, true,  false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->inputs.push_back({"Value", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Out",  PinType::Exec, false, true});
    };
    addControlYawInput.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        float value = 0.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Value") value = (float)atof(p.defaultValue.c_str());
        }
        if (!target) return;
        target->AddControlYawInput(value);
    };
    NodeRegistry::Get().RegisterNode(addControlYawInput);

    NodeTemplate addControlPitchInput;
    addControlPitchInput.type = "Add_Control_Pitch_Input";
    addControlPitchInput.category = "Actor";
    addControlPitchInput.moduleName = "Stargraph";
    addControlPitchInput.createPins = [](Node* node) {
        node->inputs.push_back({"In",    PinType::Exec, true,  false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->inputs.push_back({"Value", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Out",  PinType::Exec, false, true});
    };
    addControlPitchInput.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        float value = 0.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Value") value = (float)atof(p.defaultValue.c_str());
        }
        if (!target) return;
        target->AddControlPitchInput(value);
    };
    NodeRegistry::Get().RegisterNode(addControlPitchInput);

    NodeTemplate addControlRollInput;
    addControlRollInput.type = "Add_Control_Roll_Input";
    addControlRollInput.category = "Actor";
    addControlRollInput.moduleName = "Stargraph";
    addControlRollInput.createPins = [](Node* node) {
        node->inputs.push_back({"In",    PinType::Exec, true,  false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->inputs.push_back({"Value", PinType::Float, true, false, 0, "0.0"});
        node->outputs.push_back({"Out",  PinType::Exec, false, true});
    };
    addControlRollInput.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        float value = 0.0f;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
            }
            if (p.name == "Value") value = (float)atof(p.defaultValue.c_str());
        }
        if (!target) return;
        target->AddControlRollInput(value);
    };
    NodeRegistry::Get().RegisterNode(addControlRollInput);

    NodeTemplate getControlRotation;
    getControlRotation.type = "Get_Control_Rotation";
    getControlRotation.category = "Actor";
    getControlRotation.moduleName = "Stargraph";
    getControlRotation.createPins = [](Node* node) {
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Rotation", PinType::Vec3, false, true});
    };
    getControlRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
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
            target->controlRotation.x, target->controlRotation.y, target->controlRotation.z);
        for (Pin& p : node.outputs) if (p.name == "Rotation") p.defaultValue = buf;
    };
    NodeRegistry::Get().RegisterNode(getControlRotation);

    NodeTemplate setControlRotation;
    setControlRotation.type = "Set_Control_Rotation";
    setControlRotation.category = "Actor";
    setControlRotation.moduleName = "Stargraph";
    setControlRotation.createPins = [](Node* node) {
        node->inputs.push_back({"In",       PinType::Exec, true,  false});
        node->inputs.push_back({"Target",   PinType::Object, true,  false, 0, ""});
        node->inputs.push_back({"Rotation", PinType::Vec3, true,  false, 0, "0,0,0"});
        node->outputs.push_back({"Out",     PinType::Exec, false, true});
    };
    setControlRotation.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
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
                    &target->controlRotation.x, &target->controlRotation.y, &target->controlRotation.z);
                break;
            }
        }
        if (target->controlRotation.x > 89.0f) target->controlRotation.x = 89.0f;
        if (target->controlRotation.x < -89.0f) target->controlRotation.x = -89.0f;
    };
    NodeRegistry::Get().RegisterNode(setControlRotation);

    NodeTemplate setViewTarget;
    setViewTarget.type = "Set_View_Target";
    setViewTarget.category = "Actor";
    setViewTarget.moduleName = "Stargraph";
    setViewTarget.createPins = [](Node* node) {
        node->inputs.push_back({"In", PinType::Exec, true, false});
        node->inputs.push_back({"Target", PinType::Object, true, false, 0, ""});
        node->outputs.push_back({"Out", PinType::Exec, false, true});
    };
    setViewTarget.execute = [](Node& node, Stargraph& graph, ExecutionContext& ctx) {
        Actor* target = ctx.actor;
        for (const Pin& p : node.inputs) {
            if (p.name == "Target" && !p.defaultValue.empty()) {
                target = (Actor*)strtoull(p.defaultValue.c_str(), nullptr, 16);
                break;
            }
        }
        if (target) World::Get().SetViewTarget(target);
    };
    NodeRegistry::Get().RegisterNode(setViewTarget);
}