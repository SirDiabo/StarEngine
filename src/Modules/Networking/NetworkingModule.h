#pragma once
#include "Core/Module.h"
#include "LibDataChannelBackend.h"

class NetworkingModule : public Module {
public:
    NetworkingModule();
    ~NetworkingModule() override;

    bool Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    static NetworkingModule* Instance() { return instance; }
    LibDataChannelBackend& GetBackend() { return backend; }

    void BeginSession();
    void EndSession();

private:
    void ApplyPendingSpawns();
    void ApplyLifecycleEvents();

    static NetworkingModule* instance;
    LibDataChannelBackend backend;
    bool sessionActive = false;
};
