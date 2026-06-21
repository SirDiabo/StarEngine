#pragma once
#include "Actor.h"

class Pawn : public Actor {
public:
    Pawn();

    const char* GetClassName() const override { return "Pawn"; }
    void GetProperties(PropertyList& outProperties) override;

    bool autoPossess = false;
};
