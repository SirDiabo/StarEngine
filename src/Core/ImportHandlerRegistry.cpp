#include "ImportHandlerRegistry.h"
#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

std::string LowerExtension(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return ext;
}

}

bool ImportResult::HasProblems() const {
    for (const ImportMessage& m : messages) {
        if (m.level != ImportMessageLevel::Info) return true;
    }
    return false;
}

ImportHandlerRegistry& ImportHandlerRegistry::Get() {
    static ImportHandlerRegistry instance;
    return instance;
}

void ImportHandlerRegistry::Register(const ImportHandler& handler) {
    for (ImportHandler& existing : handlers) {
        if (existing.extension == handler.extension) {
            existing = handler;
            return;
        }
    }
    handlers.push_back(handler);
}

const ImportHandler* ImportHandlerRegistry::Find(const std::string& extension) const {
    std::string wanted = extension;
    std::transform(wanted.begin(), wanted.end(), wanted.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    for (const ImportHandler& handler : handlers) {
        if (handler.extension == wanted) return &handler;
    }
    return nullptr;
}

ImportResult ImportHandlerRegistry::Import(const std::string& sourcePath, const std::string& targetFolder) const {
    ImportResult result;
    const ImportHandler* handler = Find(LowerExtension(sourcePath));
    if (!handler || !handler->import) {
        result.Error("No import handler is registered for " + fs::path(sourcePath).extension().string());
        result.summary = "Import failed: unrecognized file type";
        return result;
    }

    std::error_code ec;
    if (!fs::exists(sourcePath, ec)) {
        result.Error("The source file does not exist: " + sourcePath);
        result.summary = "Import failed: source file not found";
        return result;
    }
    fs::create_directories(targetFolder, ec);

    ImportContext context;
    context.sourcePath = sourcePath;
    context.targetFolder = targetFolder;
    context.baseName = fs::path(sourcePath).stem().string();

    try {
        result = handler->import(context);
    } catch (const std::exception& e) {
        result = ImportResult();
        result.Error(std::string("The importer failed unexpectedly: ") + e.what());
    } catch (...) {
        result = ImportResult();
        result.Error("The importer failed unexpectedly");
    }

    std::vector<std::string> existing;
    for (const std::string& created : result.createdAssets) {
        if (fs::exists(created, ec)) existing.push_back(created);
        else result.Warn("Reported asset was not written: " + created);
    }
    result.createdAssets = existing;

    if (result.success && result.createdAssets.empty()) result.Warn("The importer finished but created no assets");
    if (result.summary.empty()) {
        if (!result.success) result.summary = "Import failed: " + context.baseName;
        else if (result.createdAssets.size() == 1) result.summary = "Imported " + fs::path(result.createdAssets[0]).stem().string();
        else result.summary = "Imported " + context.baseName + " (" + std::to_string(result.createdAssets.size()) + " assets)";
    }
    if (!result.success) {
        bool hasError = false;
        for (const ImportMessage& m : result.messages) hasError |= m.level == ImportMessageLevel::Error;
        if (!hasError) result.Error("The importer reported failure without a reason");
    }
    return result;
}

std::string ImportHandlerRegistry::UniquePath(const std::string& folder, const std::string& baseName, const std::string& extension) {
    fs::path base = fs::path(folder) / (baseName + extension);
    std::error_code ec;
    if (!fs::exists(base, ec)) return base.string();
    for (int n = 2; n < 10000; n++) {
        fs::path candidate = fs::path(folder) / (baseName + " " + std::to_string(n) + extension);
        if (!fs::exists(candidate, ec)) return candidate.string();
    }
    return base.string();
}
