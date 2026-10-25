#pragma once
#include "StarscriptAst.h"
#include "Runtime/ComponentDecl.h"
#include <memory>

namespace Starscript {

std::shared_ptr<Program> Compile(const std::string& source, const std::vector<ComponentDecl>& components);

std::string NormalizeEventName(const std::string& name);

}
