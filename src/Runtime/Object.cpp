#include "Object.h"
#include "Core/ObjectRefRegistry.h"

Object::Object() : objectId(GenerateObjectId()) {
    ObjectRefRegistry::Get().Register(this);
}

Object::~Object() {
    ObjectRefRegistry::Get().Unregister(this);
}
