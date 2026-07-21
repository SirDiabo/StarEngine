#include "World.h"
#include "Actor.h"
#include "AudioCore.h"
#include "Networking/ReplicationManager.h"
#include "Modules/Stargraph/TimerManager.h"
#include "PrintMessageLog.h"
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
    if (!actor) return;
    for (Actor* pending : pendingDestroy) {
        if (pending == actor) return;
    }
    pendingDestroy.push_back(actor);
}

void World::SetViewTarget(Actor* actor) {
    if (actor) {
        viewTarget = actor;
    } else {
        viewTarget = nullptr;
    }
}

void World::SetInputFocus(Actor* actor) {
    if (actor) {
        inputFocus = actor;
    } else {
        inputFocus = nullptr;
    }
}

uint32_t World::AllocateNetId() {
    return nextNetId++;
}

void World::BeginPlay() {
    playing = true;
    ReplicationManager::Reset();
    ResetNetIds();
    for (size_t i = 0; i < actors.size(); i++) {
        actors[i]->BeginPlay();
    }
}

void World::Tick(float deltaTime) {
    if (!playing) return;

    ReplicationManager::PreTick();

    TimerManager::Get().Tick(deltaTime);
    PrintMessageLog::Get().Tick(deltaTime);

    for (size_t i = 0; i < actors.size(); i++) {
        actors[i]->Tick(deltaTime);
    }

    ReplicationManager::PostTick();

    AudioCore::Get().TickMusicStreams();

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
    std::vector<Actor*> current;
    current.reserve(actors.size());
    for (const auto& a : actors) current.push_back(a.get());

    for (Actor* actor : current) {
        actor->EndPlay();
        DestroyActor(actor);
    }
    FlushDestroyQueue();
    playing = false;
    viewTarget = nullptr;
    ShowCursor();
    AudioCore::Get().StopAndClearMusic();
    PrintMessageLog::Get().Clear();
}

void World::FlushDestroyQueue() {
    std::vector<Actor*> toDestroy;
    toDestroy.swap(pendingDestroy);

    for (Actor* actor : toDestroy) {
        if (actor->hasBegunPlay) actor->EndPlay();
        actors.erase(std::remove_if(actors.begin(), actors.end(),
            [actor](const std::unique_ptr<Actor>& a) { return a.get() == actor; }),
            actors.end());
    }
}