#pragma once
#include <string>
#include <vector>

class Actor;

struct TriggerOverlapEntry {
    Actor* actor = nullptr;
    std::string ref;
};

namespace TriggerOverlap {

inline bool Contains(const std::vector<TriggerOverlapEntry>& list, const Actor* actor) {
    for (const TriggerOverlapEntry& e : list) {
        if (e.actor == actor) return true;
    }
    return false;
}

inline void Diff(const std::vector<TriggerOverlapEntry>& previous, const std::vector<TriggerOverlapEntry>& current,
                 std::vector<TriggerOverlapEntry>& began, std::vector<TriggerOverlapEntry>& ended) {
    began.clear();
    ended.clear();
    for (const TriggerOverlapEntry& e : current) {
        if (!Contains(previous, e.actor)) began.push_back(e);
    }
    for (const TriggerOverlapEntry& e : previous) {
        if (!Contains(current, e.actor)) ended.push_back(e);
    }
}

}
