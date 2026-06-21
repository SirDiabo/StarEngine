#include "Pawn.h"

Pawn::Pawn() {
    name = "Pawn";
}

void Pawn::GetProperties(PropertyList& outProperties) {
    Actor::GetProperties(outProperties);
    outProperties.AddBool("Auto Possess", &autoPossess);
}
