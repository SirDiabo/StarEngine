#pragma once
#include <vector>
#include <string>
#include <memory>

class Actor;

class World {
public:
    static World& Get();

    Actor* SpawnActor(const std::string& className, const std::string& name = "");
    void DestroyActor(Actor* actor);

    void BeginPlay();
    void Tick(float deltaTime);
    void EndPlay();

    bool IsPlaying() const { return playing; }

    const std::vector<std::unique_ptr<Actor>>& GetActors() const { return actors; }

    Actor* GetViewTarget() const { return viewTarget; }
    void SetViewTarget(Actor* actor);

    Actor* GetInputFocus() const { return inputFocus; }
    void SetInputFocus(Actor* actor);

private:
    std::vector<std::unique_ptr<Actor>> actors;
    std::vector<Actor*> pendingDestroy;
    bool playing = false;
    Actor* viewTarget = nullptr;
    Actor* inputFocus = nullptr;

    void FlushDestroyQueue();
};
