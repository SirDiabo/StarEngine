#pragma once
#include "Core/ProgrammingInterfaceRegistry.h"
#include "StarscriptAst.h"
#include <memory>
#include <string>

namespace Starscript {

struct WidgetCompileCache {
    std::string source;
    std::string signature;
    std::shared_ptr<Program> program;
};

class StarscriptContent : public IBehaviorContent {
public:
    std::string source;
    std::shared_ptr<Program> compiled;
    std::shared_ptr<WidgetCompileCache> widgetCache;
};

extern const char* const kInterfaceName;
extern const char* const kDefaultSource;
extern const char* const kDefaultWidgetSource;

}

void RegisterStarscriptInterface();
