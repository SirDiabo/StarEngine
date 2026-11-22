#include "PackageCooker.h"
#include "CookedText.h"
#include "StarPak.h"
#include "ObjMtlParser.h"
#include "PathSafety.h"
#include "Module.h"
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
    if (ext == ".starasset" || ext == ".starmat" || ext == ".starmap" ||
        ext == ".staractor" || ext == ".startex" || ext == ".starwidget" ||
        ext == ".staranim" || ext == ".starsound") return true;
    for (const ModuleEntry& entry : ModuleManager::Get().GetEntries()) {
        if (!entry.hasManifest) continue;
        for (const std::string& declared : entry.manifest.assetTypes) {
            if (declared == ext) return true;
        }
    }
    return false;
}

bool IsMeshSource(const fs::path& p) {
    std::string ext = p.extension().string();
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    return ext == ".obj" || ext == ".gltf" || ext == ".glb";
}

bool IsMtlSource(const fs::path& p) {
    std::string ext = p.extension().string();
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    return ext == ".mtl";
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

using CookSink = std::function<void(const std::string& virtualPath, std::vector<uint8_t>&& bytes, uint64_t rawSize, const std::string& compression)>;

CookResult CookInternal(const fs::path& contentRoot, const std::string& cipher, const std::string& cipherKey,
                         const std::string& compression, const PackageCooker::MeshCooker* meshCooker, const CookSink& sink) {
    CookResult result;
    fs::path importedDirRelative = "_Imported";

    if (!fs::exists(contentRoot)) {
        result.error = "Content folder not found: " + contentRoot.string();
        return result;
    }

    std::error_code ec;
    auto cipherImpl = CreateCookCipher(cipher);
    if (!cipherImpl) {
        result.error = "Unknown obfuscation method: " + cipher;
        return result;
    }

    auto finish = [&](const std::string& virtualPath, std::vector<uint8_t>&& bytes) {
        uint64_t rawSize = bytes.size();
        std::string used = PackCompressions::None;
        if (compression != PackCompressions::None && !compression.empty() && !bytes.empty()) {
            std::vector<uint8_t> packed;
            if (PackageCompression::Compress(compression, bytes, packed) && packed.size() < bytes.size()) {
                bytes.swap(packed);
                used = compression;
                result.filesCompressed++;
            }
        }
        cipherImpl->Encrypt(bytes, cipherKey, virtualPath);
        result.rawBytes += rawSize;
        result.storedBytes += bytes.size();
        sink(virtualPath, std::move(bytes), rawSize, used);
        result.filesCooked++;
    };

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

        if (key == "Source" && !vendoredFallback.empty()) {
            std::error_code vec;
            if (fs::exists(contentRoot / vendoredFallback, vec)) return key + "=" + vendoredFallback;
        }
        if (!exists) return line;

        fs::path relativeToContent = fs::relative(valuePath, contentRoot, ec);
        bool insideContent = !ec && relativeToContent.generic_string().rfind("..", 0) != 0;
        if (insideContent) return key + "=" + relativeToContent.generic_string();

        if (!PathSafety::IsMediaSourceExtension(valuePath)) return line;
        return key + "=" + queueImport(value);
    };

    for (const auto& entry : fs::recursive_directory_iterator(contentRoot)) {
        if (!entry.is_regular_file()) continue;

        fs::path relative = fs::relative(entry.path(), contentRoot, ec);
        std::string virtualPath = relative.generic_string();

        std::vector<uint8_t> bytes;
        if (meshCooker != nullptr && IsMtlSource(entry.path())) continue;

        if (meshCooker != nullptr && IsMeshSource(entry.path())) {
            std::string cookError;
            if (!(*meshCooker)(entry.path().u8string(), bytes, cookError)) {
                result.error = "Could not cook mesh " + virtualPath + ": " + cookError;
                return result;
            }
        } else if (IsTextProjectFile(entry.path())) {
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
            if (meshCooker != nullptr) {
                const std::vector<uint8_t> seedBytes = CookRandomBytes(4);
                const uint32_t seed = (uint32_t)seedBytes[0] | ((uint32_t)seedBytes[1] << 8) | ((uint32_t)seedBytes[2] << 16) | ((uint32_t)seedBytes[3] << 24);
                if (!CookedText::Pack(text, seed, bytes)) {
                    result.error = "Could not pack " + virtualPath;
                    return result;
                }
            } else {
                bytes.assign(text.begin(), text.end());
            }
        } else {
            bytes = ReadBytes(entry.path());
        }

        finish(virtualPath, std::move(bytes));
    }

    for (size_t i = 0; i < importQueue.size(); i++) {
        std::string source = importQueue[i].first;
        std::string virtualPath = importQueue[i].second;

        fs::path sourcePath(source);
        std::vector<uint8_t> bytes;

        if (meshCooker != nullptr && IsMeshSource(sourcePath)) {
            std::string cookError;
            if (!(*meshCooker)(sourcePath.u8string(), bytes, cookError)) {
                result.error = "Could not cook mesh " + source + ": " + cookError;
                return result;
            }
            finish(virtualPath, std::move(bytes));
            continue;
        }
        if (meshCooker != nullptr && IsMtlSource(sourcePath)) continue;

        bytes = ReadBytes(sourcePath);

        if (ObjMtlParser::IsObjFile(sourcePath)) {
            for (const std::string& mtlName : ObjMtlParser::ParseMtllibReferences(bytes)) {
                fs::path mtlPath = PathSafety::ResolveMtlReference(sourcePath.parent_path(), mtlName);
                if (!mtlPath.empty()) queueImport(mtlPath.string());
            }
        } else if (ObjMtlParser::IsMtlFile(sourcePath)) {
            for (const std::string& texName : ObjMtlParser::ParseMtlTextureReferences(bytes)) {
                fs::path texPath = PathSafety::ResolveTextureReference(sourcePath.parent_path(), texName);
                if (!texPath.empty()) queueImport(texPath.string());
            }
        }

        finish(virtualPath, std::move(bytes));
    }

    result.success = true;
    return result;
}

} // namespace

CookResult PackageCooker::CookLoose(const std::string& contentRootStr, const std::string& outputContentDirStr) {
    fs::path outputContentDir(outputContentDirStr);
    std::error_code ec;
    fs::create_directories(outputContentDir, ec);

    return CookInternal(fs::path(contentRootStr), CookCiphers::None, std::string(), PackCompressions::None, nullptr,
        [&](const std::string& virtualPath, std::vector<uint8_t>&& bytes, uint64_t, const std::string&) {
            WriteBytes(outputContentDir / virtualPath, bytes);
        });
}

CookResult PackageCooker::CookToPak(const std::string& contentRootStr, const std::string& outputPakFileStr,
                                     const std::string& cipher, const std::string& cipherKey,
                                     const std::string& bootConfigText,
                                     const std::vector<ExtraPakFile>& extraFiles,
                                     const std::string& compression,
                                     const MeshCooker& meshCooker) {
    StarPak::Writer writer;
    writer.SetIndexKey(cipherKey);

    std::string rejectedPath;
    CookResult result = CookInternal(fs::path(contentRootStr), cipher, cipherKey, compression, &meshCooker,
        [&](const std::string& virtualPath, std::vector<uint8_t>&& bytes, uint64_t rawSize, const std::string& used) {
            if (!writer.AddEntry(virtualPath, bytes, rawSize, used) && rejectedPath.empty()) rejectedPath = virtualPath;
        });
    if (!result.success) return result;
    if (!rejectedPath.empty()) {
        result.success = false;
        result.error = "A file name contains a tab or line break and cannot be packaged: " + rejectedPath;
        return result;
    }

    auto cipherImpl = CreateCookCipher(cipher);
    if (!cipherImpl) {
        result.success = false;
        result.error = "Unknown obfuscation method: " + cipher;
        return result;
    }

    std::vector<uint8_t> bootBytes(bootConfigText.begin(), bootConfigText.end());
    cipherImpl->Encrypt(bootBytes, cipherKey, StarPak::kBootConfigPath);
    writer.AddFile(StarPak::kBootConfigPath, bootBytes);

    for (const ExtraPakFile& extra : extraFiles) {
        std::vector<uint8_t> extraBytes = extra.bytes;
        cipherImpl->Encrypt(extraBytes, cipherKey, extra.virtualPath);
        if (!writer.AddFile(extra.virtualPath, extraBytes)) {
            result.success = false;
            result.error = "Invalid file name in package: " + extra.virtualPath;
            return result;
        }
    }

    if (!writer.WriteToFile(outputPakFileStr)) {
        result.success = false;
        result.error = "Could not write pak file: " + outputPakFileStr;
    }
    return result;
}
