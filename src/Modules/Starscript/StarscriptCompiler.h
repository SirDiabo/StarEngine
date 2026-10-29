#pragma once
#include "StarscriptAst.h"
#include "Runtime/ComponentDecl.h"
#include <memory>

namespace Starscript {

struct WidgetElementInfo {
    std::string id;
    std::string className;
};

std::shared_ptr<Program> Compile(const std::string& source, const std::vector<ComponentDecl>& components);
std::shared_ptr<Program> CompileWidget(const std::string& source, const std::vector<WidgetElementInfo>& elements);

std::string NormalizeEventName(const std::string& name);

}
