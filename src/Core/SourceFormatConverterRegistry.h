#pragma once
#include <string>
#include <functional>
#include <vector>

struct SourceFormatConverter {
    std::string extension;
    std::string moduleName;
    std::string label;
    std::function<bool(const std::string& srcPath, const std::string& outputDir,
                        std::string& outConvertedPath, std::string& outError)> convert;
};

class SourceFormatConverterRegistry {
public:
    static SourceFormatConverterRegistry& Get();

    void Register(const SourceFormatConverter& converter);

    std::vector<SourceFormatConverter> GetAvailableConverters() const;
    const SourceFormatConverter* FindAvailable(const std::string& extension) const;

private:
    std::vector<SourceFormatConverter> converters;
};
