#include "RenderingModule.h"

RenderingModule::RenderingModule() {
    name = "Rendering";
}

bool RenderingModule::Initialize() {
    return true;
}

void RenderingModule::Shutdown() {}
void RenderingModule::Update(float deltaTime) {}
