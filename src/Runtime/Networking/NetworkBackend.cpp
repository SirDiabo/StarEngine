#include "NetworkBackend.h"

NetworkBackendRegistry& NetworkBackendRegistry::Get() {
    static NetworkBackendRegistry instance;
    return instance;
}

void NetworkBackendRegistry::Register(NetworkBackend* backend) {
    active.store(backend);
}

void NetworkBackendRegistry::Unregister(NetworkBackend* backend) {
    if (active.load() == backend) active.store(nullptr);
}
