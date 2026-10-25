#include "World.h"
#include "ProfilerLabels.h"
#include "Core/Profiler.h"
#include "Actor.h"
#include "SaveGame.h"
#include "Core/SaveFileFormat.h"
#include "AudioCore.h"
#include "Networking/ReplicationManager.h"
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

void World::AddPreTickHook(const std::string& name, PreTickHook hook) {
    for (NamedPreTickHook& existing : preTickHooks) {
        if (existing.name == name) { existing.hook = std::move(hook); return; }
    }
    preTickHooks.push_back({ name, std::move(hook) });
}

void World::RemovePreTickHook(const std::string& name) {
    for (size_t i = 0; i < preTickHooks.size(); i++) {
        if (preTickHooks[i].name != name) continue;
        preTickHooks.erase(preTickHooks.begin() + (long)i);
        return;
    }
}

void World::BeginPlay() {
    SaveFileFormat::InvalidateCache();
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

    for (size_t i = 0; i < preTickHooks.size(); i++) {
        PROFILE_SCOPE_DYN(preTickHooks[i].name);
        preTickHooks[i].hook(deltaTime);
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
            if (!IsCursorHidden()) HideCursor();
            const int centerX = GetScreenWidth() / 2;
            const int centerY = GetScreenHeight() / 2;
            Vector2 mouse = GetMousePosition();
            if ((int)mouse.x != centerX || (int)mouse.y != centerY) SetMousePosition(centerX, centerY);
        } else if (IsCursorHidden()) {
            ShowCursor();
        }
    }

    {
        PROFILE_SCOPE("Destroy Queue");
        FlushDestroyQueue();
    }
}

void World::EndPlay() {
    SaveFileFormat::FlushAll();
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
