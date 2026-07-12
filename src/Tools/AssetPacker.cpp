#include "Core/StarPak.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: AssetPacker <assets_dir> <target_exe>\n";
        return 1;
    }

    fs::path assetsDir = argv[1];
    fs::path targetExe = argv[2];

    if (!fs::exists(assetsDir) || !fs::is_directory(assetsDir)) {
        std::cerr << "AssetPacker: assets dir not found: " << assetsDir << "\n";
        return 1;
    }
    if (!fs::exists(targetExe)) {
        std::cerr << "AssetPacker: target exe not found: " << targetExe << "\n";
        return 1;
    }

    StarPak::Writer writer;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(assetsDir, ec)) {
        if (!entry.is_regular_file()) continue;

        fs::path rel = fs::relative(entry.path(), assetsDir, ec);
        if (ec) continue;
        std::string virtualPath = "Assets/" + rel.generic_string();

        std::ifstream in(entry.path(), std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "AssetPacker: could not read " << entry.path() << "\n";
            return 1;
        }
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        writer.AddFile(virtualPath, bytes);
    }

    fs::path pakPath = targetExe;
    pakPath += ".editorassets.tmp";
    if (!writer.WriteToFile(pakPath.string())) {
        std::cerr << "AssetPacker: failed to write intermediate pak " << pakPath << "\n";
        return 1;
    }

    bool ok = StarPak::AppendPakToExecutable(targetExe.string(), pakPath.string());
    fs::remove(pakPath, ec);

    if (!ok) {
        std::cerr << "AssetPacker: failed to append pak to " << targetExe << "\n";
        return 1;
    }

    return 0;
}
