#include "StarscriptModule.h"
#include "StarscriptInterface.h"

StarscriptModule::StarscriptModule() {
    name = "Starscript";
}

bool StarscriptModule::Initialize() {
    RegisterStarscriptInterface();
    return true;
}

void StarscriptModule::Shutdown() {}

void StarscriptModule::Update(float deltaTime) {}
