#include "PackageCooker.h"
#include "StarPak.h"
#include "ObjMtlParser.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool IsTextProjectFile(const fs::path& p) {
    std::string ext = p.extension().string();
    return ext == ".starasset" || ext == ".starmat" || ext == ".starmap" ||
           ext == ".staractor" || ext == ".stargraph" || ext == ".startex" ||
           ext == ".starwidget";
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

using CookSink = std::function<void(const std::string& virtualPath, std::vector<uint8_t>&& bytes)>;

CookResult CookInternal(const fs::path& contentRoot, CookCipher cipher, const std::string& cipherKey,
                         const CookSink& sink) {
    CookResult result;
    fs::path importedDirRelative = "_Imported";

    if (!fs::exists(contentRoot)) {
        result.error = "Content folder not found: " + contentRoot.string();
        return result;
    }

    std::error_code ec;
    auto cipherImpl = CreateCookCipher(cipher);

    std::unordered_map<std::string, std::string> importedNames; // absolute source path -> virtual path
    std::unordered_set<std::string> reservedImportedNames;
    std::vector<std::pair<std::string, std::string>> importQueue; // absolute source path, virtual path

    auto queueImport = [&](const std::string& absoluteSource) -> std::string {
        auto found = importedNames.find(absoluteSource);
        if (found != importedNames.end()) return found->second;

        std::string name = UniqueImportedName(fs::path(absoluteSource), reservedImportedNames);
        std::string virtualPath = (importedDirRelative / name).generic_string();
        importedNames.emplace(absoluteSource, virtualPath);
        importQueue.emplace_back(absoluteSource, virtualPath);
        return virtualPath;
    };

    auto rewriteLine = [&](const std::string& line, const std::string& vendoredFallback) -> std::string {
        size_t sep = line.find('=');
        if (sep == std::string::npos) return line;

        std::string key = line.substr(0, sep);
        std::string value = line.substr(sep + 1);
        if (value.empty()) return line;

        fs::path valuePath(value);
        bool exists = valuePath.is_absolute() && fs::exists(valuePath);

        if (!exists && key == "Source" && !vendoredFallback.empty()) {
            std::error_code vec;
            if (fs::exists(contentRoot / vendoredFallback, vec)) return key + "=" + vendoredFallback;
        }
        if (!exists) return line;

        fs::path relativeToContent = fs::relative(valuePath, contentRoot, ec);
        bool insideContent = !ec && relativeToContent.generic_string().rfind("..", 0) != 0;
        if (insideContent) return key + "=" + relativeToContent.generic_string();

        return key + "=" + queueImport(value);
    };

    for (const auto& entry : fs::recursive_directory_iterator(contentRoot)) {
        if (!entry.is_regular_file()) continue;

        fs::path relative = fs::relative(entry.path(), contentRoot, ec);
        std::string virtualPath = relative.generic_string();

        std::vector<uint8_t> bytes;
        if (IsTextProjectFile(entry.path())) {
            std::ifstream in(entry.path());
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(in, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                lines.push_back(line);
            }

            std::string vendoredFallback;
            for (const std::string& l : lines) {
                if (l.rfind("VendoredSource=", 0) == 0) { vendoredFallback = l.substr(15); break; }
            }

            std::ostringstream rewritten;
            for (const std::string& l : lines) rewritten << rewriteLine(l, vendoredFallback) << "\n";
            std::string text = rewritten.str();
            bytes.assign(text.begin(), text.end());
        } else {
            bytes = ReadBytes(entry.path());
        }

        cipherImpl->Transform(bytes, cipherKey);
        sink(virtualPath, std::move(bytes));
        result.filesCooked++;
    }

    for (size_t i = 0; i < importQueue.size(); i++) {
        std::string source = importQueue[i].first;
        std::string virtualPath = importQueue[i].second;

        fs::path sourcePath(source);
        std::vector<uint8_t> bytes = ReadBytes(sourcePath);

        if (ObjMtlParser::IsObjFile(sourcePath)) {
            for (const std::string& mtlName : ObjMtlParser::ParseMtllibReferences(bytes)) {
                fs::path mtlPath = sourcePath.parent_path() / mtlName;
                if (fs::exists(mtlPath)) queueImport(mtlPath.string());
            }
        } else if (ObjMtlParser::IsMtlFile(sourcePath)) {
            for (const std::string& texName : ObjMtlParser::ParseMtlTextureReferences(bytes)) {
                fs::path texPath = sourcePath.parent_path() / texName;
                if (fs::exists(texPath)) queueImport(texPath.string());
            }
        }

        cipherImpl->Transform(bytes, cipherKey);
        sink(virtualPath, std::move(bytes));
        result.filesCooked++;
    }

    result.success = true;
    return result;
}

} // namespace

CookResult PackageCooker::CookLoose(const std::string& contentRootStr, const std::string& outputContentDirStr,
                                     CookCipher cipher, const std::string& cipherKey) {
    fs::path outputContentDir(outputContentDirStr);
    std::error_code ec;
    fs::create_directories(outputContentDir, ec);

    return CookInternal(fs::path(contentRootStr), cipher, cipherKey,
        [&](const std::string& virtualPath, std::vector<uint8_t>&& bytes) {
            WriteBytes(outputContentDir / virtualPath, bytes);
        });
}

CookResult PackageCooker::CookToPak(const std::string& contentRootStr, const std::string& outputPakFileStr,
                                     CookCipher cipher, const std::string& cipherKey,
                                     const std::string& bootConfigText,
                                     const std::vector<ExtraPakFile>& extraFiles) {
    StarPak::Writer writer;

    CookResult result = CookInternal(fs::path(contentRootStr), cipher, cipherKey,
        [&](const std::string& virtualPath, std::vector<uint8_t>&& bytes) {
            writer.AddFile(virtualPath, bytes);
        });
    if (!result.success) return result;

    writer.AddFile(StarPak::kBootConfigPath,
                   std::vector<uint8_t>(bootConfigText.begin(), bootConfigText.end()));

    for (const ExtraPakFile& extra : extraFiles) {
        writer.AddFile(extra.virtualPath, extra.bytes);
    }

    if (!writer.WriteToFile(outputPakFileStr)) {
        result.success = false;
        result.error = "Could not write pak file: " + outputPakFileStr;
    }
    return result;
}
