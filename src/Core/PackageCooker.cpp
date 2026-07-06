#include "PackageCooker.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool IsTextProjectFile(const fs::path& p) {
    std::string ext = p.extension().string();
    return ext == ".starasset" || ext == ".starmat" || ext == ".starmap" ||
           ext == ".staractor" || ext == ".stargraph";
}

std::vector<uint8_t> ReadBytes(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& p, const std::vector<uint8_t>& bytes) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
}

std::string UniqueImportedName(const fs::path& source, std::unordered_set<std::string>& reserved) {
    std::string base = source.stem().string();
    std::string ext = source.extension().string();
    std::string name = base + ext;
    int suffix = 1;
    while (reserved.count(name)) {
        name = base + "_" + std::to_string(suffix++) + ext;
    }
    reserved.insert(name);
    return name;
}

} // namespace

CookResult PackageCooker::Cook(const std::string& contentRootStr, const std::string& outputContentDirStr,
                                 CookCipher cipher, const std::string& cipherKey) {
    CookResult result;
    fs::path contentRoot(contentRootStr);
    fs::path outputContentDir(outputContentDirStr);
    fs::path importedDirRelative = "_Imported";

    if (!fs::exists(contentRoot)) {
        result.error = "Content folder not found: " + contentRootStr;
        return result;
    }

    std::error_code ec;
    fs::create_directories(outputContentDir, ec);

    std::unordered_map<std::string, std::string> importedNames;
    std::unordered_set<std::string> reservedImportedNames;
    
    std::vector<std::pair<std::string, std::string>> pendingImports;

    auto rewriteLine = [&](const std::string& line) -> std::string {
        size_t sep = line.find('=');
        if (sep == std::string::npos) return line;

        std::string key = line.substr(0, sep);
        std::string value = line.substr(sep + 1);
        if (value.empty()) return line;

        fs::path valuePath(value);
        if (!valuePath.is_absolute() || !fs::exists(valuePath)) return line;

        fs::path relativeToContent = fs::relative(valuePath, contentRoot, ec);
        bool insideContent = !ec && relativeToContent.generic_string().rfind("..", 0) != 0;
        if (insideContent) {
            return key + "=" + relativeToContent.generic_string();
        }

        auto found = importedNames.find(value);
        if (found != importedNames.end()) return key + "=" + found->second;

        std::string name = UniqueImportedName(valuePath, reservedImportedNames);
        std::string relativeDest = (importedDirRelative / name).generic_string();
        importedNames.emplace(value, relativeDest);
        pendingImports.emplace_back(value, (outputContentDir / importedDirRelative / name).string());
        return key + "=" + relativeDest;
    };

    auto cipherImpl = CreateCookCipher(cipher);

    for (const auto& entry : fs::recursive_directory_iterator(contentRoot)) {
        if (!entry.is_regular_file()) continue;

        fs::path relative = fs::relative(entry.path(), contentRoot, ec);
        fs::path destPath = outputContentDir / relative;

        std::vector<uint8_t> bytes;
        if (IsTextProjectFile(entry.path())) {
            std::ifstream in(entry.path());
            std::ostringstream rewritten;
            std::string line;
            while (std::getline(in, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                rewritten << rewriteLine(line) << "\n";
            }
            std::string text = rewritten.str();
            bytes.assign(text.begin(), text.end());
        } else {
            bytes = ReadBytes(entry.path());
        }

        cipherImpl->Transform(bytes, cipherKey);
        WriteBytes(destPath, bytes);
        result.filesCooked++;
    }

    for (const auto& [source, dest] : pendingImports) {
        std::vector<uint8_t> bytes = ReadBytes(source);
        cipherImpl->Transform(bytes, cipherKey);
        WriteBytes(dest, bytes);
        result.filesCooked++;
    }

    result.success = true;
    return result;
}
