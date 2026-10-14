#pragma once
#include <string>

class Actor;

namespace SaveGame {

void SaveActor(Actor* actor, const std::string& onlyFile = "");
void LoadActor(Actor* actor, const std::string& onlyFile = "");

void SaveWorld(const std::string& onlyFile = "");
void LoadWorld(const std::string& onlyFile = "");

bool SaveVariable(Actor* actor, const std::string& variableName, const std::string& file = "");
bool LoadVariable(Actor* actor, const std::string& variableName, const std::string& file = "");

}
