#include "World.h"
#include "Actor.h"
#include "Pawn.h"
#include "Character.h"
#include "raylib.h"
#include <algorithm>

World& World::Get() {
    static World instance;
    return instance;
}

Actor* World::SpawnActor(const std::string& className, const std::string& name) {
    std::unique_ptr<Actor> actor;

    if (className == "Character") actor = std::make_unique<Character>();
    else if (className == "Pawn")  actor = std::make_unique<Pawn>();
    else                           actor = std::make_unique<Actor>();

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
    for (auto& actor : actors) {
        actor->BeginPlay();
    }
}

void World::Tick(float deltaTime) {
    if (!playing) return;
    for (auto& actor : actors) {
        actor->Tick(deltaTime);
    }
    FlushDestroyQueue();
}

void World::EndPlay() {
    for (auto& actor : actors) {
        actor->EndPlay();
        DestroyActor(actor.get());
    }
    FlushDestroyQueue();
    playing = false;
}

void World::FlushDestroyQueue() {
    for (Actor* actor : pendingDestroy) {
        actors.erase(std::remove_if(actors.begin(), actors.end(),
            [actor](const std::unique_ptr<Actor>& a) { return a.get() == actor; }),
            actors.end());
    }
    pendingDestroy.clear();
}
