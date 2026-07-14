#include "ModuleImporter.h"
#include "ModuleManifest.h"
#include <cstdio>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

static bool RunCommand(const std::string& cmd, std::string& outOutput) {
    std::string fullCmd = cmd + " 2>&1";
    FILE* pipe = nullptr;
#ifdef _WIN32
    pipe = _popen(fullCmd.c_str(), "r");
#else
    pipe = popen(fullCmd.c_str(), "r");
#endif
    if (!pipe) {
        outOutput = "Failed to start process";
        return false;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        outOutput += buffer;
    }

#ifdef _WIN32
    int exitCode = _pclose(pipe);
#else
    int exitCode = pclose(pipe);
#endif
    return exitCode == 0;
}

std::string ModuleImporter::GetCatalogUrl() {
    return "https://raw.githubusercontent.com/starengine-modules/catalog/main/catalog.txt";
}

bool ModuleImporter::FetchCatalog(std::vector<CatalogEntry>& outEntries, std::string& outError) {
    std::string output;
    std::string cmd = "curl -fsSL \"" + GetCatalogUrl() + "\"";
    if (!RunCommand(cmd, output)) {
        outError = "Could not reach the module catalog. Check your connection.";
        return false;
    }

    outEntries.clear();
    std::istringstream stream(output);
    std::string line;
    CatalogEntry current;
    bool hasContent = false;

    auto flush = [&]() {
        if (hasContent && !current.name.empty()) outEntries.push_back(current);
        current = CatalogEntry();
        hasContent = false;
    };

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.empty()) {
            flush();
            continue;
        }

        size_t sep = line.find('=');
        if (sep == std::string::npos) continue;

        std::string key = line.substr(0, sep);
        std::string value = line.substr(sep + 1);
        hasContent = true;

        if (key == "name") current.name = value;
        else if (key == "repository") current.repository = value;
        else if (key == "category") current.category = value;
        else if (key == "description") current.description = value;
    }
    flush();

    if (outEntries.empty()) {
        outError = "Catalog returned no modules.";
        return false;
    }
    return true;
}

bool ModuleImporter::CloneFromRepository(const std::string& ownerRepo,
                                          const std::string& projectModulesDir,
                                          std::string& outError) {
    size_t slash = ownerRepo.find('/');
    if (slash == std::string::npos || slash == 0 || slash == ownerRepo.size() - 1) {
        outError = "Expected a repository in \"owner/repo\" form.";
        return false;
    }
    std::string repoName = ownerRepo.substr(slash + 1);

    fs::path destDir = fs::path(projectModulesDir) / repoName;
    if (fs::exists(destDir)) {
        outError = "A module already exists at Modules/" + repoName;
        return false;
    }

    std::error_code ec;
    fs::create_directories(projectModulesDir, ec);

    std::string url = "https://github.com/" + ownerRepo + ".git";
    std::string output;
    std::string cmd = "git clone --depth 1 --recurse-submodules \"" + url + "\" \"" + destDir.string() + "\"";
    if (!RunCommand(cmd, output)) {
        outError = "Clone failed: " + output;
        fs::remove_all(destDir, ec);
        return false;
    }

    ModuleManifest manifest;
    bool foundManifest = false;
    for (const auto& fileEntry : fs::directory_iterator(destDir, ec)) {
        if (fileEntry.path().extension() == ".starmodule") {
            foundManifest = ModuleManifest::LoadFromFile(fileEntry.path().string(), manifest);
            break;
        }
    }
    if (!foundManifest) {
        outError = "Cloned " + ownerRepo + " but it has no .starmodule manifest at its root.";
        fs::remove_all(destDir, ec);
        return false;
    }

    // Record where this came from so the module list can offer updates later.
    manifest.repository = ownerRepo;
    for (const auto& fileEntry : fs::directory_iterator(destDir, ec)) {
        if (fileEntry.path().extension() == ".starmodule") {
            ModuleManifest::SaveToFile(fileEntry.path().string(), manifest);
            break;
        }
    }

    return true;
}

bool ModuleImporter::CopyFromDisk(const std::string& manifestPath,
                                   const std::string& projectModulesDir,
                                   std::string& outError) {
    ModuleManifest manifest;
    if (!ModuleManifest::LoadFromFile(manifestPath, manifest)) {
        outError = "Failed to read manifest: " + manifestPath;
        return false;
    }

    fs::path sourceDir = fs::path(manifestPath).parent_path();
    fs::path destDir = fs::path(projectModulesDir) / manifest.name;

    std::error_code ec;
    if (fs::exists(destDir) && fs::equivalent(sourceDir, destDir, ec)) {
        outError = "Module already lives in this project's Modules folder.";
        return false;
    }
    if (fs::exists(destDir)) {
        outError = "A module named \"" + manifest.name + "\" is already imported.";
        return false;
    }

    fs::create_directories(destDir.parent_path(), ec);
    fs::copy(sourceDir, destDir, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

    if (ec) {
        outError = "Import failed: " + ec.message();
        return false;
    }
    return true;
}
