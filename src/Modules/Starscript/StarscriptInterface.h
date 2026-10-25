#pragma once
#include "Core/ProgrammingInterfaceRegistry.h"
#include "StarscriptAst.h"
#include <memory>
#include <string>

namespace Starscript {

class StarscriptContent : public IBehaviorContent {
public:
    std::string source;
    std::shared_ptr<Program> compiled;
};

extern const char* const kInterfaceName;
extern const char* const kDefaultSource;

}

void RegisterStarscriptInterface();
