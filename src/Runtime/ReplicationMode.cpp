#include "ReplicationMode.h"

std::string ReplicationModeToString(ReplicationMode mode) {
    switch (mode) {
        case ReplicationMode::Replicated: return "Replicated";
        case ReplicationMode::ServerOnly: return "ServerOnly";
        default: return "Local";
    }
}

ReplicationMode ReplicationModeFromString(const std::string& text) {
    if (text == "Replicated") return ReplicationMode::Replicated;
    if (text == "ServerOnly") return ReplicationMode::ServerOnly;
    return ReplicationMode::Local;
}
