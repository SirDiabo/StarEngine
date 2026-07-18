#include "SaveFileFormat.h"
#include "SavePaths.h"
#include "SaveFileRegistry.h"
#include "PackageCipher.h"
#include "Project.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {
    std::unordered_map<std::string, std::string> ParseEntries(const std::string& text) {
        std::unordered_map<std::string, std::string> entries;
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            size_t separator = line.find('=');
            if (separator == std::string::npos) continue;

            entries[line.substr(0, separator)] = line.substr(separator + 1);
        }
        return entries;
    }

    std::string SerializeEntries(const std::unordered_map<std::string, std::string>& entries) {
        std::string text;
        for (const auto& kv : entries) {
            text += kv.first;
            text += "=";
            text += kv.second;
            text += "\n";
        }
        return text;
    }

    std::vector<uint8_t> ReadRawFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return {};
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    void WriteRawFile(const std::string& path, const std::vector<uint8_t>& data) {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) return;
        out.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
    }
}

std::unordered_map<std::string, std::string> SaveFileFormat::ReadEntries(const std::string& saveFileName) {
    std::vector<uint8_t> raw = ReadRawFile(SavePaths::GetSaveFilePath(saveFileName));
    if (raw.empty()) return {};

    if (SaveFileRegistry::Get().IsEncrypted(saveFileName)) {
        auto cipher = CreateCookCipher(CookCipher::XorSimple);
        cipher->Transform(raw, Project::Get().GetSettings().saveEncryptionKey);
    }

    std::string text(raw.begin(), raw.end());
    return ParseEntries(text);
}

bool SaveFileFormat::WriteEntries(const std::string& saveFileName, const std::unordered_map<std::string, std::string>& updates) {
    std::unordered_map<std::string, std::string> entries = ReadEntries(saveFileName);
    for (const auto& kv : updates) entries[kv.first] = kv.second;

    std::string text = SerializeEntries(entries);
    std::vector<uint8_t> raw(text.begin(), text.end());

    if (SaveFileRegistry::Get().IsEncrypted(saveFileName)) {
        auto cipher = CreateCookCipher(CookCipher::XorSimple);
        cipher->Transform(raw, Project::Get().GetSettings().saveEncryptionKey);
    }

    std::string path = SavePaths::GetSaveFilePath(saveFileName);
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    WriteRawFile(path, raw);
    return true;
}
