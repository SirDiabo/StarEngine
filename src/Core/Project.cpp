#include "Project.h"
#include "Module.h"
#include "LayoutSerializer.h"
#include "NamedListRegistry.h"
#include "SaveFileRegistry.h"
#include "InputMappingRegistry.h"
#include "RenderSettings.h"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include "SafeParse.h"
#include <unordered_map>
#include <random>

namespace fs = std::filesystem;

static std::string GetStarEngineHomeDir() {
    std::string home;
#ifdef _WIN32
    char* buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, "USERPROFILE") == 0 && buf) {
        home = buf;
        free(buf);
    } else {
        home = ".";
    }
#else
    const char* h = getenv("HOME");
    home = h ? h : ".";
#endif
    fs::path dir = fs::path(home) / ".starengine";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string();
}

std::string GameRendererName(GameRenderer renderer) {
    return renderer == GameRenderer::Software ? "Software" : "Hardware";
}

GameRenderer GameRendererFromName(const std::string& name) {
    return name == "Software" ? GameRenderer::Software : GameRenderer::Hardware;
}

static std::string GenerateSaveEncryptionKey() {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, (int)sizeof(alphabet) - 2);
    std::string key(24, ' ');
    for (char& c : key) c = alphabet[dist(gen)];
    return key;
}

Project& Project::Get() {
    static Project instance;
    return instance;
}

std::string Project::GetProjectsDirectory() {
    const char* home = getenv("USERPROFILE");
    if (!home) home = getenv("HOME");

    fs::path docsPath = home ? home : ".";
    docsPath /= "Documents";
    docsPath /= "StarProjects";
    return docsPath.string();
}

bool Project::Load(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    projectFilePath = path;
    rootPath = fs::path(path).parent_path().string();
    settings = ProjectSettings();
    settings.name = GetFileNameWithoutExt(path.c_str());

    std::string line;
    while (std::getline(in, line)) {
        size_t separator = line.find('=');
        if (separator == std::string::npos) continue;

        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        if (!value.empty() && value.back() == '\r') value.pop_back();

        if (key == "name") settings.name = value;
        else if (key == "targetWidth") settings.targetWidth = SafeParse::ToInt(value);
        else if (key == "targetHeight") settings.targetHeight = SafeParse::ToInt(value);
        else if (key == "defaultMap") settings.defaultMapPath = value;
        else if (key == "renderer") settings.renderer = GameRendererFromName(value);
        else if (key == "saveEncryptionKey") settings.saveEncryptionKey = value;
        else if (key == "disabledModules") {
            settings.disabledModules.clear();
            std::stringstream ss(value);
            std::string moduleName;
            while (std::getline(ss, moduleName, ',')) {
                if (!moduleName.empty()) settings.disabledModules.push_back(moduleName);
            }
        }
    }

    ModuleManager::Get().ApplyDisabledModuleNames(settings.disabledModules);
    NamedListRegistry::Get().Load(rootPath);
    SaveFileRegistry::Get().Load(rootPath);
    InputMappingRegistry::Get().Load(rootPath);
    RenderSettings::Get().Load(rootPath);

    std::error_code ec;
    fs::create_directories(fs::path(GetModulesDirectory()), ec);
    WriteActiveProjectPointer();

    if (settings.saveEncryptionKey.empty()) {
        settings.saveEncryptionKey = GenerateSaveEncryptionKey();
        Save();
    }

    return true;
}

std::string Project::GetModulesDirectory() const {
    return (fs::path(rootPath) / "Modules").string();
}

void Project::WriteActiveProjectPointer() const {
    std::unordered_map<std::string, std::string> data;
    data["ProjectRoot"] = rootPath;
    LayoutSerializer::Save((fs::path(GetStarEngineHomeDir()) / "active_project.cfg").string(), data);
}

void Project::Save() {
    if (projectFilePath.empty()) return;

    settings.disabledModules = ModuleManager::Get().GetDisabledModuleNames();

    std::ofstream out(projectFilePath);
    if (!out.is_open()) return;

    out << "name=" << settings.name << "\n";
    out << "targetWidth=" << settings.targetWidth << "\n";
    out << "targetHeight=" << settings.targetHeight << "\n";
    out << "defaultMap=" << settings.defaultMapPath << "\n";
    out << "renderer=" << GameRendererName(settings.renderer) << "\n";
    out << "saveEncryptionKey=" << settings.saveEncryptionKey << "\n";

    out << "disabledModules=";
    for (size_t i = 0; i < settings.disabledModules.size(); i++) {
        if (i > 0) out << ",";
        out << settings.disabledModules[i];
    }
    out << "\n";
}