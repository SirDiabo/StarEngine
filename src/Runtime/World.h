#pragma once
#include <vector>
#include <string>
#include <memory>
#include <functional>

class Actor;

class World {
public:
    static World& Get();

    Actor* SpawnActor(const std::string& className, const std::string& name = "");
    Actor* SpawnActorDeferred(const std::string& className, const std::string& name = "");
    void FinishSpawningActor(Actor* actor);
    void DestroyActor(Actor* actor);

    void BeginPlay();
    void Tick(float deltaTime);
    void EndPlay();

    bool IsPlaying() const { return playing; }

    void SaveGame(const std::string& saveFile = "");
    void LoadGame(const std::string& saveFile = "");

    const std::vector<std::unique_ptr<Actor>>& GetActors() const { return actors; }
    bool IsActorValid(Actor* actor) const;

    Actor* GetViewTarget() const { return viewTarget; }
    void SetViewTarget(Actor* actor);

    Actor* GetInputFocus() const { return inputFocus; }
    void SetInputFocus(Actor* actor);

    uint32_t AllocateNetId();
    void ResetNetIds() { nextNetId = 1; }

    const std::string& GetCurrentLevelName() const { return currentLevelName; }
    void SetCurrentLevelName(const std::string& name) { currentLevelName = name; }

    void RequestLoadLevel(const std::string& path);
    bool ConsumePendingLevelLoad(std::string& outPath);

    void RequestQuit();
    bool ConsumeQuitRequest();

    using PreTickHook = std::function<void(float deltaTime)>;
    void AddPreTickHook(const std::string& name, PreTickHook hook);
    void RemovePreTickHook(const std::string& name);

private:
    std::vector<std::unique_ptr<Actor>> actors;
    std::vector<Actor*> pendingDestroy;
    bool playing = false;
    Actor* viewTarget = nullptr;
    Actor* inputFocus = nullptr;
    uint32_t nextNetId = 1;
    std::string currentLevelName;
    std::string pendingLevelPath;
    bool hasPendingLevelLoad = false;
    bool quitRequested = false;

    struct NamedPreTickHook {
        std::string name;
        PreTickHook hook;
    };
    std::vector<NamedPreTickHook> preTickHooks;

    void FlushDestroyQueue();
};
