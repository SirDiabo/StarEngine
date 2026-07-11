#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

// Sits between World and whichever NetworkBackend is active. Diffs Replicated/ServerOnly variables every tick and applies/relays whatever the backend hands back.
namespace ReplicationManager {
    // Apply incoming updates from the backend.
    void PreTick();

    // Diff replicated variables and send anything that changed.
    void PostTick();

    // Drops all cached "last sent" values. Call when a networking session starts or ends.
    void Reset();
}
