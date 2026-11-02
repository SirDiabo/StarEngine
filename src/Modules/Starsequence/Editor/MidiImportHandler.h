#pragma once
#include "Core/ImportHandlerRegistry.h"

namespace Starsequence {

ImportResult RunMidiImport(const ImportContext& context);
std::vector<ImportOptionDef> MidiImportOptions();
void RegisterMidiImportHandlers();

}
