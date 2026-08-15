#include "ModuleImporter.h"
#include "ModuleManifest.h"
#include "SafeProcess.h"
#include <cstdio>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

static bool RunCommand(const std::string& cmd, std::string& outOutput) {
    return SafeProcess::Run(SafeProcess::TokenizeCommand(cmd), outOutput) == 0;
}

static bool MoveDirectory(const fs::path& from, const fs::path& to, std::string& outError) {
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);

    fs::rename(from, to, ec);
    if (!ec) return true;

    ec.clear();
    fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) {
        outError = "Move failed: " + ec.message();
        return false;
    }
    fs::remove_all(from, ec);
    return true;
}

static bool IsValidRepoId(const std::string& ownerRepo) {
    size_t slash = ownerRepo.find('/');
    if (slash == std::string::npos || slash == 0 || slash == ownerRepo.size() - 1) return false;
    if (ownerRepo.find('/', slash + 1) != std::string::npos) return false;
    return SafeProcess::IsShellSafe(ownerRepo);
}

std::string ModuleImporter::GetCatalogUrl() {
    return "https://raw.githubusercontent.com/sirdiabo/starengine-catalog/main/catalog.txt";
}

bool ModuleImporter::FetchCatalog(std::vector<CatalogEntry>& outEntries, std::string& outError) {
    std::string output;
    std::vector<std::string> argv = {"curl", "-fsSL", GetCatalogUrl()};
    if (SafeProcess::Run(argv, output) != 0) {
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
        else if (key == "engineModule") current.engineModule = (value == "true");
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
                                           std::string& outDestDir,
                                           std::string& outError) {
    if (!IsValidRepoId(ownerRepo)) {
        outError = "Expected a repository in \"owner/repo\" form (letters, digits, . _ - only).";
        return false;
    }
    std::string repoName = ownerRepo.substr(ownerRepo.find('/') + 1);

    fs::path destDir = fs::path(projectModulesDir) / repoName;
    if (fs::exists(destDir)) {
        outError = "A module already exists at Modules/" + repoName;
        return false;
    }

    std::error_code ec;
    fs::create_directories(projectModulesDir, ec);

    std::string url = "https://github.com/" + ownerRepo + ".git";
    std::string output;
    std::vector<std::string> argv = {"git", "clone", "--depth", "1", "--recurse-submodules", url, destDir.string()};
    if (SafeProcess::Run(argv, output) != 0) {
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

    manifest.repository = ownerRepo;
    for (const auto& fileEntry : fs::directory_iterator(destDir, ec)) {
        if (fileEntry.path().extension() == ".starmodule") {
            ModuleManifest::SaveToFile(fileEntry.path().string(), manifest);
            break;
        }
    }

    outDestDir = destDir.string();
    return true;
}

bool ModuleImporter::RelocateModule(const std::string& fromDir,
                                     const std::string& toModulesDir,
                                     std::string& outDestDir,
                                     std::string& outError) {
    fs::path from(fromDir);
    fs::path to = fs::path(toModulesDir) / from.filename();

    if (fs::exists(to)) {
        outError = "A module already exists at " + to.string();
        return false;
    }

    if (!MoveDirectory(from, to, outError)) return false;

    outDestDir = to.string();
    return true;
}

bool ModuleImporter::CopyFromDisk(const std::string& manifestPath,
                                   const std::string& projectModulesDir,
                                   std::string& outDestDir,
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

    if (fs::exists(destDir / ".git")) {
        std::string gitOutput;
        std::vector<std::string> gitArgs = {"git", "-C", destDir.string(), "submodule", "update", "--init", "--recursive"};
        int rc = SafeProcess::Run(gitArgs, gitOutput);
        if (rc != 0) {
            outError = "Imported, but failed to init submodules (git rc=" + std::to_string(rc) + "): " + gitOutput;
            return false;
        }
    }

    outDestDir = destDir.string();
    return true;
}
