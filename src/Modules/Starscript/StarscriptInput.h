#pragma once
#include <string>

class Actor;

namespace Starscript {

struct InputReading {
    bool held = false;
    float axis = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
};

bool IsKnownInputName(const std::string& name);
InputReading SampleInput(Actor* owner, const std::string& name);

}
