#pragma once
#include <string>
#include <vector>

class Actor;
class StargraphBehavior;
struct Node;

class TimerManager {
public:
    static TimerManager& Get();

    void Tick(float deltaTime);

    // Node-keyed timers
    bool IsNodeTimerActive(StargraphBehavior* owner, Node* node) const;
    void CancelNodeTimers(StargraphBehavior* owner, Node* node);
    void StartNodeTimer(StargraphBehavior* owner, Actor* actor, Node* node, const std::string& outPin, float duration);

    // Named timers
    void StartNamedTimer(StargraphBehavior* owner, Actor* actor, const std::string& functionName,
                          float rate, bool loop, bool limitToFrameRate);
    void PauseNamedTimer(StargraphBehavior* owner, const std::string& functionName);
    void ResumeNamedTimer(StargraphBehavior* owner, const std::string& functionName);
    void ClearNamedTimer(StargraphBehavior* owner, const std::string& functionName);
    bool IsNamedTimerActive(StargraphBehavior* owner, const std::string& functionName) const;

    void ClearAllTimersForOwner(StargraphBehavior* owner);

private:
    TimerManager() = default;

    struct PendingTimer {
        StargraphBehavior* owner = nullptr;
        Actor* actor = nullptr;
        Node* node = nullptr;
        std::string outPin;
        std::string functionName;
        float remaining = 0.0f;
        float interval = 0.0f;
        bool loop = false;
        bool limitToFrameRate = false;
        bool paused = false;
    };

    std::vector<PendingTimer> timers;
};
