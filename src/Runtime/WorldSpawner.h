#pragma once
#include "StarMap.h"

class Actor;

namespace WorldSpawner {
    Actor* SpawnActorFromInstance(const ActorInstance& inst);
    void SpawnMapActors(const StarMap& map);
}
