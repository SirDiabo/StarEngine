#include "NetworkBackend.h"

NetworkBackendRegistry& NetworkBackendRegistry::Get() {
    static NetworkBackendRegistry instance;
    return instance;
}

void NetworkBackendRegistry::Register(NetworkBackend* backend) {
    active = backend;
}

void NetworkBackendRegistry::Unregister(NetworkBackend* backend) {
    if (active == backend) active = nullptr;
}
