#include "World.h"
#include "ProfilerLabels.h"
#include "Core/Profiler.h"
#include "Actor.h"
#include "SaveGame.h"
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

Actor* World::SpawnActorDeferred(const std::string& className, const std::string& name) {
    std::unique_ptr<Actor> actor = std::make_unique<Actor>();

    if (!name.empty()) actor->SetName(name);
    if (!className.empty()) actor->SetActorClass(className);

    Actor* raw = actor.get();
    actors.push_back(std::move(actor));

    return raw;
}

void World::FinishSpawningActor(Actor* actor) {
    if (playing && actor && !actor->hasBegunPlay) actor->BeginPlay();
}

Actor* World::SpawnActor(const std::string& className, const std::string& name) {
    Actor* raw = SpawnActorDeferred(className, name);
    FinishSpawningActor(raw);
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

void World::RequestLoadLevel(const std::string& path) {
    pendingLevelPath = path;
    hasPendingLevelLoad = true;
}

bool World::ConsumePendingLevelLoad(std::string& outPath) {
    if (!hasPendingLevelLoad) return false;
    outPath = pendingLevelPath;
    hasPendingLevelLoad = false;
    pendingLevelPath.clear();
    return true;
}

void World::RequestQuit() {
    quitRequested = true;
}

bool World::ConsumeQuitRequest() {
    if (!quitRequested) return false;
    quitRequested = false;
    return true;
}

void World::SaveGame(const std::string& saveFile) {
    ::SaveGame::SaveWorld(saveFile);
}

void World::LoadGame(const std::string& saveFile) {
    ::SaveGame::LoadWorld(saveFile);
}

void World::BeginPlay() {
    playing = true;
    Profiler::NotifyPlayState(true);
    ReplicationManager::Reset();
    ResetNetIds();
    for (size_t i = 0; i < actors.size(); i++) {
        if (!actors[i]->hasBegunPlay) actors[i]->BeginPlay();
    }
}

void World::Tick(float deltaTime) {
    if (!playing) return;
    PROFILE_SCOPE("World Tick");

    {
        PROFILE_SCOPE("Replication Pre-Tick");
        ReplicationManager::PreTick();
    }

    {
        PROFILE_SCOPE("Timers");
        TimerManager::Get().Tick(deltaTime);
    }
    {
        PROFILE_SCOPE("Print Message Log");
        PrintMessageLog::Get().Tick(deltaTime);
    }

    {
        PROFILE_SCOPE("Actors");
        for (size_t i = 0; i < actors.size(); i++) {
            PROFILE_SCOPE_DYN(ProfilerLabels::ForActor(*actors[i]));
            actors[i]->Tick(deltaTime);
        }
    }

    {
        PROFILE_SCOPE("Replication Post-Tick");
        ReplicationManager::PostTick();
    }

    {
        PROFILE_SCOPE("Audio Music Streams");
        AudioCore::Get().TickMusicStreams();
    }

    {
        PROFILE_SCOPE("Mouse Cursor");
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
    }

    {
        PROFILE_SCOPE("Destroy Queue");
        FlushDestroyQueue();
    }
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
    Profiler::NotifyPlayState(false);
    viewTarget = nullptr;
    ShowCursor();
    AudioCore::Get().StopAndClearMusic();
    AudioCore::Get().StopAllSounds();
    PrintMessageLog::Get().Clear();
}

bool World::IsActorValid(Actor* actor) const {
    if (!actor) return false;
    for (const auto& a : actors) if (a.get() == actor) return true;
    return false;
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
