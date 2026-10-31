#pragma once
#include "SequenceData.h"
#include <string>
#include <vector>

namespace Starsequence {

struct ParseDiagnostic {
    int line = 0;
    bool error = false;
    std::string message;
};

struct ParseResult {
    Sequence sequence;
    bool ok = false;
    std::vector<ParseDiagnostic> diagnostics;

    bool HasErrors() const;
};

ParseResult ParseSequence(const std::string& text);
std::string WriteSequence(const Sequence& sequence);

bool LoadSequenceFile(const std::string& path, ParseResult& result);
bool SaveSequenceFile(const std::string& path, const Sequence& sequence, std::string& error);

Sequence MakeDefaultSequence(const std::string& name);

const char* ControllerCode(ControllerType type);

}
