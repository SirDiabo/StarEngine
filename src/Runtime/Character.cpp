#include "Character.h"

Character::Character() {
    name = "Character";
}

void Character::GetProperties(PropertyList& outProperties) {
    Pawn::GetProperties(outProperties);
    outProperties.AddFloat("Max Walk Speed", &maxWalkSpeed);
    outProperties.AddFloat("Jump Height", &jumpHeight);
    outProperties.AddFloat("Capsule Radius", &capsuleRadius);
    outProperties.AddFloat("Capsule Half Height", &capsuleHalfHeight);
}
