#include "World.h"
#include "Actor.h"
#include "raylib.h"
#include <algorithm>

World& World::Get() {
    static World instance;
    return instance;
}

Actor* World::SpawnActor(const std::string& className, const std::string& name) {
    std::unique_ptr<Actor> actor = std::make_unique<Actor>();

    if (!name.empty()) actor->SetName(name);

    Actor* raw = actor.get();
    actors.push_back(std::move(actor));

    if (playing) raw->BeginPlay();

    return raw;
}

void World::DestroyActor(Actor* actor) {
    pendingDestroy.push_back(actor);
}

void World::BeginPlay() {
    playing = true;
    for (size_t i = 0; i < actors.size(); i++) {
        actors[i]->BeginPlay();
    }
}

void World::Tick(float deltaTime) {
    if (!playing) return;
    for (size_t i = 0; i < actors.size(); i++) {
        actors[i]->Tick(deltaTime);
    }

    bool shouldLockMouse = false;
    for (const auto& actor : actors) {
        if (!actor->showMouseCursor) {
            shouldLockMouse = true;
            break;
        }
    }
    if (shouldLockMouse) {
        HideCursor();
        SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
    } else {
        ShowCursor();
    }

    FlushDestroyQueue();
}

void World::EndPlay() {
    for (size_t i = 0; i < actors.size(); i++) {
        actors[i]->EndPlay();
        DestroyActor(actors[i].get());
    }
    FlushDestroyQueue();
    playing = false;
    ShowCursor();
}

void World::FlushDestroyQueue() {
    for (Actor* actor : pendingDestroy) {
        actors.erase(std::remove_if(actors.begin(), actors.end(),
            [actor](const std::unique_ptr<Actor>& a) { return a.get() == actor; }),
            actors.end());
    }
    pendingDestroy.clear();
}