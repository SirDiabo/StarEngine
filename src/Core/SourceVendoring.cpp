#include "SourceVendoring.h"
#include "ObjMtlParser.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

bool CopyFileInto(const fs::path& src, const fs::path& destDir) {
    std::error_code ec;
    fs::create_directories(destDir, ec);
    fs::copy_file(src, destDir / src.filename(), fs::copy_options::overwrite_existing, ec);
    return !ec;
}

std::vector<uint8_t> ReadBytes(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}

std::string SourceVendoring::Vendor(const std::string& contentRootStr, const std::string& absoluteSourcePathStr,
                                     const std::string& assetName) {
    fs::path contentRoot(contentRootStr);
    fs::path source(absoluteSourcePathStr);
    std::error_code ec;
    if (!fs::exists(source, ec)) return "";

    fs::path vendorDir = contentRoot / "_Source" / assetName;
    if (!CopyFileInto(source, vendorDir)) return "";

    if (ObjMtlParser::IsObjFile(source)) {
        std::vector<uint8_t> objBytes = ReadBytes(source);
        for (const std::string& mtlName : ObjMtlParser::ParseMtllibReferences(objBytes)) {
            fs::path mtlPath = source.parent_path() / mtlName;
            if (!fs::exists(mtlPath, ec)) continue;
            CopyFileInto(mtlPath, vendorDir);

            std::vector<uint8_t> mtlBytes = ReadBytes(mtlPath);
            for (const std::string& texName : ObjMtlParser::ParseMtlTextureReferences(mtlBytes)) {
                fs::path texPath = mtlPath.parent_path() / texName;
                if (fs::exists(texPath, ec)) CopyFileInto(texPath, vendorDir);
            }
        }
    }

    fs::path relative = fs::relative(vendorDir / source.filename(), contentRoot, ec);
    if (ec) return "";
    return relative.generic_string();
}
