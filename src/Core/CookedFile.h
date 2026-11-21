#pragma once
#include "PackageCipher.h"
#include "StarPak.h"
#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <vector>

namespace CookedFile {
    void SetActivePak(StarPak::Reader&& pak, const std::string& cipher, const std::string& key);
    bool IsUsingPak();

    std::string DiskPath(const std::string& path);
    std::string Key(const std::string& path);
    bool Exists(const std::string& path);
    long ModifiedTime(const std::string& path);

    std::shared_ptr<const std::vector<uint8_t>> ReadShared(const std::string& path);
    bool ReadBytes(const std::string& path, std::vector<uint8_t>& out);
    bool ReadText(const std::string& path, std::string& out);
    std::unique_ptr<std::istream> OpenStream(const std::string& path);
}
