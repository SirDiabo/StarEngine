#include "StarPak.h"
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

constexpr char kMagicV1[8] = {'S', 'T', 'A', 'R', 'P', 'A', 'K', '1'};
constexpr char kMagic[8] = {'S', 'T', 'A', 'R', 'P', 'A', 'K', '2'};
constexpr char kFooterMagic[8] = {'S', 'T', 'A', 'R', 'P', 'A', 'K', 'F'};

#pragma pack(push, 1)
struct Footer {
    char magic[8];
    uint64_t pakOffset;
    uint64_t pakSize;
};
#pragma pack(pop)

std::vector<std::string> SplitTabs(const std::string& line) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (true) {
        size_t tab = line.find('\t', start);
        if (tab == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    return fields;
}

std::string ParentOf(const std::string& virtualPath) {
    size_t slash = virtualPath.find_last_of('/');
    return slash == std::string::npos ? std::string() : virtualPath.substr(0, slash);
}

} // namespace

namespace StarPak {

void Writer::AddFile(const std::string& virtualPath, const std::vector<uint8_t>& bytes) {
    AddEntry(virtualPath, bytes, bytes.size(), PackCompression::None);
}

void Writer::AddEntry(const std::string& virtualPath, const std::vector<uint8_t>& storedBytes, uint64_t rawSize, PackCompression compression) {
    Entry e;
    e.path = virtualPath;
    e.offset = blob.size();
    e.size = storedBytes.size();
    e.rawSize = rawSize;
    e.compression = compression;
    entries.push_back(std::move(e));
    blob.insert(blob.end(), storedBytes.begin(), storedBytes.end());
}

bool Writer::WriteToFile(const std::string& outputPath) const {
    std::error_code ec;
    fs::create_directories(fs::path(outputPath).parent_path(), ec);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) return false;

    out.write(kMagic, sizeof(kMagic));
    out << "\n" << entries.size() << "\n";
    for (const auto& e : entries) {
        out << e.path << "\t" << e.offset << "\t" << e.size << "\t" << e.rawSize << "\t" << (int)e.compression << "\n";
    }
    out.write(reinterpret_cast<const char*>(blob.data()), (std::streamsize)blob.size());
    return out.good();
}

bool Reader::ParseFromBuffer(std::vector<uint8_t>&& buffer) {
    entries.clear();
    indexByPath.clear();
    blob.clear();

    size_t pos = 0;
    auto readLine = [&](std::string& line) -> bool {
        if (pos >= buffer.size()) return false;
        size_t nl = pos;
        while (nl < buffer.size() && buffer[nl] != '\n') nl++;
        line.assign(reinterpret_cast<const char*>(buffer.data()) + pos, nl - pos);
        pos = (nl < buffer.size()) ? nl + 1 : nl;
        return true;
    };

    if (buffer.size() < sizeof(kMagic)) return false;
    bool versionOne = std::memcmp(buffer.data(), kMagicV1, sizeof(kMagicV1)) == 0;
    if (!versionOne && std::memcmp(buffer.data(), kMagic, sizeof(kMagic)) != 0) return false;
    pos = sizeof(kMagic);
    if (pos < buffer.size() && buffer[pos] == '\n') pos++;

    std::string countLine;
    if (!readLine(countLine)) return false;
    size_t count = 0;
    try {
        count = (size_t)std::stoull(countLine);
    } catch (...) {
        return false;
    }

    entries.reserve(count);
    for (size_t i = 0; i < count; i++) {
        std::string line;
        if (!readLine(line)) return false;

        std::vector<std::string> fields = SplitTabs(line);
        size_t required = versionOne ? 3 : 5;
        if (fields.size() < required) return false;

        Entry e;
        e.path = fields[0];
        try {
            e.offset = std::stoull(fields[1]);
            e.size = std::stoull(fields[2]);
            if (versionOne) {
                e.rawSize = e.size;
            } else {
                e.rawSize = std::stoull(fields[3]);
                int method = std::stoi(fields[4]);
                if (!IsKnownPackCompression(method)) return false;
                e.compression = (PackCompression)method;
            }
        } catch (...) {
            return false;
        }

        indexByPath.emplace(e.path, entries.size());
        entries.push_back(std::move(e));
    }

    blob.assign(buffer.begin() + (std::ptrdiff_t)pos, buffer.end());
    return true;
}

bool Reader::LoadFile(const std::string& pakPath) {
    std::ifstream in(pakPath, std::ios::binary);
    if (!in.is_open()) return false;

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return ParseFromBuffer(std::move(buffer));
}

bool Reader::LoadBuffer(const std::vector<uint8_t>& buffer) {
    return ParseFromBuffer(std::vector<uint8_t>(buffer));
}

bool Reader::LoadFromExecutable(const std::string& exePath) {
    std::ifstream in(exePath, std::ios::binary);
    if (!in.is_open()) return false;

    in.seekg(0, std::ios::end);
    std::streamoff fileSize = in.tellg();
    if (fileSize < (std::streamoff)sizeof(Footer)) return false;

    Footer footer{};
    in.seekg(fileSize - (std::streamoff)sizeof(Footer));
    in.read(reinterpret_cast<char*>(&footer), sizeof(Footer));
    if (!in) return false;
    if (std::memcmp(footer.magic, kFooterMagic, sizeof(kFooterMagic)) != 0) return false;
    if (footer.pakSize == 0 || footer.pakOffset > (uint64_t)fileSize) return false;
    if (footer.pakOffset + footer.pakSize != (uint64_t)fileSize - sizeof(Footer)) return false;

    std::vector<uint8_t> buffer(footer.pakSize);
    in.seekg((std::streamoff)footer.pakOffset);
    in.read(reinterpret_cast<char*>(buffer.data()), (std::streamsize)footer.pakSize);
    if (!in) return false;

    return ParseFromBuffer(std::move(buffer));
}

bool Reader::HasEntry(const std::string& virtualPath) const {
    return indexByPath.count(virtualPath) != 0;
}

bool Reader::GetEntryInfo(const std::string& virtualPath, Entry& out) const {
    auto it = indexByPath.find(virtualPath);
    if (it == indexByPath.end()) return false;
    out = entries[it->second];
    return true;
}

std::vector<uint8_t> Reader::ReadEntry(const std::string& virtualPath) const {
    auto it = indexByPath.find(virtualPath);
    if (it == indexByPath.end()) return {};

    const Entry& e = entries[it->second];
    if (e.offset + e.size > blob.size()) return {};
    return std::vector<uint8_t>(blob.begin() + e.offset, blob.begin() + e.offset + e.size);
}

std::vector<std::string> Reader::SiblingsOf(const std::string& virtualPath) const {
    std::string parent = ParentOf(virtualPath);
    std::vector<std::string> result;
    for (const auto& e : entries) {
        if (ParentOf(e.path) == parent) result.push_back(e.path);
    }
    return result;
}

bool AppendPakToExecutable(const std::string& exePath, const std::string& pakPath) {
    std::ifstream pakIn(pakPath, std::ios::binary);
    if (!pakIn.is_open()) return false;
    std::vector<uint8_t> pakBytes((std::istreambuf_iterator<char>(pakIn)), std::istreambuf_iterator<char>());
    pakIn.close();

    std::error_code ec;
    uint64_t originalSize = (uint64_t)fs::file_size(exePath, ec);
    if (ec) return false;

    uint64_t pakOffset = originalSize;
    if (originalSize >= sizeof(Footer)) {
        std::ifstream peek(exePath, std::ios::binary);
        peek.seekg((std::streamoff)(originalSize - sizeof(Footer)));
        Footer existing{};
        peek.read(reinterpret_cast<char*>(&existing), sizeof(Footer));
        if (peek && std::memcmp(existing.magic, kFooterMagic, sizeof(kFooterMagic)) == 0) {
            pakOffset = existing.pakOffset;
        }
    }

    std::ofstream out(exePath, std::ios::binary | std::ios::app);
    if (!out.is_open()) return false;

    out.write(reinterpret_cast<const char*>(pakBytes.data()), (std::streamsize)pakBytes.size());

    Footer footer{};
    std::memcpy(footer.magic, kFooterMagic, sizeof(kFooterMagic));
    footer.pakOffset = pakOffset;
    footer.pakSize = pakBytes.size();
    out.write(reinterpret_cast<const char*>(&footer), sizeof(footer));

    return out.good();
}

}
