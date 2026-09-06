#pragma once
#include <string>

enum class ReplicationMode {
    Local,          // Never leaves the machine.
    Replicated,     // Client Authoritative. Writes sync up to host, then from host to everyone.
    ServerOnly      // Write-Gated to Host, syncs out to clients. Clients can't write and only read this.
};

std::string ReplicationModeToString(ReplicationMode mode);
ReplicationMode ReplicationModeFromString(const std::string& text);
