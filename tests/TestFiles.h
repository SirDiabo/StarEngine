#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>
#include <vector>

namespace test {

namespace fs = std::filesystem;

class TempDir {
public:
    TempDir() {
        std::random_device rd;
        uint64_t token = ((uint64_t)rd() << 32) ^ (uint64_t)rd() ^ (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
        const char* digits = "0123456789abcdef";
        std::string suffix;
        for (int i = 0; i < 16; i++) suffix += digits[(token >> (i * 4)) & 15];
        std::error_code ec;
        path = fs::weakly_canonical(fs::temp_directory_path(ec), ec) / ("starengine_test_" + suffix);
        fs::create_directories(path, ec);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    fs::path Path(const std::string& relative) const { return path / relative; }
    std::string Str(const std::string& relative) const { return (path / relative).string(); }

    fs::path path;
};

inline bool WriteFile(const fs::path& file, const std::vector<uint8_t>& bytes) {
    std::error_code ec;
    if (file.has_parent_path()) fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
    return out.good();
}

inline bool WriteText(const fs::path& file, const std::string& text) {
    return WriteFile(file, std::vector<uint8_t>(text.begin(), text.end()));
}

inline std::vector<uint8_t> ReadFile(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in.is_open()) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

inline std::string ReadText(const fs::path& file) {
    std::vector<uint8_t> bytes = ReadFile(file);
    return std::string(bytes.begin(), bytes.end());
}

}
