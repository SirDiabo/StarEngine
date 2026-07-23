#include "TimerManager.h"
#include "StargraphBehavior.h"
#include "GraphExecutor.h"
#include "Runtime/World.h"
#include "raylib.h"
#include <algorithm>

static const size_t kMaxTimers = 4096;
static const int kMaxCatchUpFires = 1000;

TimerManager& TimerManager::Get() {
    static TimerManager instance;
    return instance;
}

bool TimerManager::IsNodeTimerActive(StargraphBehavior* owner, Node* node) const {
    for (const PendingTimer& t : timers) {
        if (t.owner == owner && t.node == node) return true;
    }
    return false;
}

void TimerManager::CancelNodeTimers(StargraphBehavior* owner, Node* node) {
    timers.erase(std::remove_if(timers.begin(), timers.end(), [owner, node](const PendingTimer& t) {
        return t.owner == owner && t.node == node;
    }), timers.end());
}

void TimerManager::StartNodeTimer(StargraphBehavior* owner, Actor* actor, Node* node, const std::string& outPin, float duration) {
    if (timers.size() >= kMaxTimers) {
        TraceLog(LOG_WARNING, "TimerManager: timer limit reached, ignoring new Delay");
        return;
    }

    PendingTimer t;
    t.owner = owner;
    t.actor = actor;
    t.node = node;
    t.outPin = outPin;
    t.remaining = duration;
    timers.push_back(t);
}

void TimerManager::StartNamedTimer(StargraphBehavior* owner, Actor* actor, const std::string& functionName,
                                    float rate, bool loop, bool limitToFrameRate) {
    ClearNamedTimer(owner, functionName);

    if (timers.size() >= kMaxTimers) {
        TraceLog(LOG_WARNING, "TimerManager: timer limit reached, ignoring new Timer by Function Name");
        return;
    }

    PendingTimer t;
    t.owner = owner;
    t.actor = actor;
    t.functionName = functionName;
    t.remaining = rate;
    t.interval = rate;
    t.loop = loop;
    t.limitToFrameRate = limitToFrameRate;
    timers.push_back(t);
}

void TimerManager::PauseNamedTimer(StargraphBehavior* owner, const std::string& functionName) {
    for (PendingTimer& t : timers) {
        if (t.owner == owner && t.functionName == functionName) t.paused = true;
    }
}

void TimerManager::ResumeNamedTimer(StargraphBehavior* owner, const std::string& functionName) {
    for (PendingTimer& t : timers) {
        if (t.owner == owner && t.functionName == functionName) t.paused = false;
    }
}

void TimerManager::ClearNamedTimer(StargraphBehavior* owner, const std::string& functionName) {
    timers.erase(std::remove_if(timers.begin(), timers.end(), [owner, &functionName](const PendingTimer& t) {
        return t.owner == owner && t.functionName == functionName;
    }), timers.end());
}

bool TimerManager::IsNamedTimerActive(StargraphBehavior* owner, const std::string& functionName) const {
    for (const PendingTimer& t : timers) {
        if (t.owner == owner && t.functionName == functionName) return true;
    }
    return false;
}

void TimerManager::ClearAllTimersForOwner(StargraphBehavior* owner) {
    timers.erase(std::remove_if(timers.begin(), timers.end(), [owner](const PendingTimer& t) {
        return t.owner == owner;
    }), timers.end());
}

void TimerManager::Tick(float deltaTime) {
    struct FireEvent {
        StargraphBehavior* owner;
        Actor* actor;
        Node* node;
        std::string outPin;
        std::string functionName;
    };
    std::vector<FireEvent> firing;

    World& world = World::Get();
    timers.erase(std::remove_if(timers.begin(), timers.end(), [&world](const PendingTimer& t) {
        return t.actor && !world.IsActorValid(t.actor);
    }), timers.end());

    // Decrement pass
    size_t count = timers.size();
    for (size_t i = 0; i < count; i++) {
        if (!timers[i].paused) timers[i].remaining -= deltaTime;
    }

    // Collect expirations and settle loop state before anything fires
    for (size_t i = 0; i < count; i++) {
        PendingTimer& t = timers[i];
        if (t.paused || t.remaining > 0.0f) continue;

        int fires = 0;
        while (t.remaining <= 0.0f && fires < kMaxCatchUpFires) {
            firing.push_back({t.owner, t.actor, t.node, t.outPin, t.functionName});
            fires++;
            if (!t.loop) break;
            t.remaining += t.interval;
            if (t.limitToFrameRate) { t.remaining = t.interval; break; }
        }
    }

    timers.erase(std::remove_if(timers.begin(), timers.end(), [](const PendingTimer& t) {
        return !t.loop && t.remaining <= 0.0f;
    }), timers.end());

    for (FireEvent& f : firing) {
        ExecutionContext ctx;
        ctx.actor = f.actor;
        ctx.deltaTime = deltaTime;
        if (f.node) {
            GraphExecutor::RunFromPin(f.owner->GetGraph(), f.node, ctx, f.outPin);
        } else {
            f.owner->OnCustomEvent(f.actor, f.functionName, {});
        }
    }
}