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

private:
    std::vector<std::unique_ptr<Actor>> actors;
    std::vector<Actor*> pendingDestroy;
    bool playing = false;

    void FlushDestroyQueue();
};
