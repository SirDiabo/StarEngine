#pragma once
#include "StarscriptAst.h"

namespace Starscript {

std::unique_ptr<Program> Parse(const std::string& source);

}
