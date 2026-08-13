#include "SourceFormatConverterRegistry.h"
#include "Module.h"

SourceFormatConverterRegistry& SourceFormatConverterRegistry::Get() {
    static SourceFormatConverterRegistry instance;
    return instance;
}

void SourceFormatConverterRegistry::Register(const SourceFormatConverter& converter) {
    for (SourceFormatConverter& existing : converters) {
        if (existing.extension == converter.extension) {
            existing = converter;
            return;
        }
    }
    converters.push_back(converter);
}

static bool IsConverterAvailable(const SourceFormatConverter& converter) {
    return converter.moduleName.empty() || ModuleManager::Get().IsModuleEnabled(converter.moduleName);
}

std::vector<SourceFormatConverter> SourceFormatConverterRegistry::GetAvailableConverters() const {
    std::vector<SourceFormatConverter> result;
    for (const SourceFormatConverter& converter : converters) {
        if (IsConverterAvailable(converter)) result.push_back(converter);
    }
    return result;
}

const SourceFormatConverter* SourceFormatConverterRegistry::FindAvailable(const std::string& extension) const {
    for (const SourceFormatConverter& converter : converters) {
        if (converter.extension != extension) continue;
        return IsConverterAvailable(converter) ? &converter : nullptr;
    }
    return nullptr;
}
