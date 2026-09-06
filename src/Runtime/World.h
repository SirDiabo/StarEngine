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
    bool IsActorValid(Actor* actor) const;

    Actor* GetViewTarget() const { return viewTarget; }
    void SetViewTarget(Actor* actor);

    Actor* GetInputFocus() const { return inputFocus; }
    void SetInputFocus(Actor* actor);

    uint32_t AllocateNetId();
    void ResetNetIds() { nextNetId = 1; }

    void RequestLoadLevel(const std::string& path);
    bool ConsumePendingLevelLoad(std::string& outPath);

    void RequestQuit();
    bool ConsumeQuitRequest();

private:
    std::vector<std::unique_ptr<Actor>> actors;
    std::vector<Actor*> pendingDestroy;
    bool playing = false;
    Actor* viewTarget = nullptr;
    Actor* inputFocus = nullptr;
    uint32_t nextNetId = 1;
    std::string pendingLevelPath;
    bool hasPendingLevelLoad = false;
    bool quitRequested = false;

    void FlushDestroyQueue();
};