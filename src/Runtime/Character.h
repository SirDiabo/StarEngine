#pragma once
#include "Pawn.h"

class Character : public Pawn {
public:
    Character();

    const char* GetClassName() const override { return "Character"; }
    void GetProperties(PropertyList& outProperties) override;

    float maxWalkSpeed = 400.0f;
    float jumpHeight = 50.0f;
    float capsuleRadius = 50.0f;
    float capsuleHalfHeight = 100.0f;
};
