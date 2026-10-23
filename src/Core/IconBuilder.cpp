#include "IconBuilder.h"
#include "raylib.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace {

void PutU16(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back((uint8_t)(value & 0xff));
    out.push_back((uint8_t)((value >> 8) & 0xff));
}

void PutU32(std::vector<uint8_t>& out, uint32_t value) {
    for (int i = 0; i < 4; i++) out.push_back((uint8_t)((value >> (8 * i)) & 0xff));
}

std::vector<int> ChooseSizes(int side) {
    static const int standard[] = { 16, 24, 32, 48, 64, 128, 256 };
    std::vector<int> sizes;
    int top = std::min(side, IconBuilder::kMaxIconSize);
    for (int size : standard) {
        if (size <= top) sizes.push_back(size);
    }
    if (std::find(sizes.begin(), sizes.end(), top) == sizes.end()) sizes.push_back(top);
    return sizes;
}

bool EncodePng(const Image& image, std::vector<uint8_t>& out) {
    int length = 0;
    unsigned char* data = ExportImageToMemory(image, ".png", &length);
    if (!data || length <= 0) {
        if (data) MemFree(data);
        return false;
    }
    out.assign(data, data + length);
    MemFree(data);
    return true;
}

}

bool IconBuilder::WriteBytes(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::error_code ec;
    fs::path target(path);
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    std::ofstream out(target, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
    return out.good();
}

std::vector<uint8_t> IconBuilder::AssembleIco(const std::vector<std::vector<uint8_t>>& pngImages, const std::vector<int>& sizes) {
    std::vector<uint8_t> ico;
    size_t count = std::min(pngImages.size(), sizes.size());

    PutU16(ico, 0);
    PutU16(ico, 1);
    PutU16(ico, (uint32_t)count);

    uint32_t offset = 6 + 16 * (uint32_t)count;
    for (size_t i = 0; i < count; i++) {
        uint8_t dimension = sizes[i] >= 256 ? 0 : (uint8_t)sizes[i];
        ico.push_back(dimension);
        ico.push_back(dimension);
        ico.push_back(0);
        ico.push_back(0);
        PutU16(ico, 1);
        PutU16(ico, 32);
        PutU32(ico, (uint32_t)pngImages[i].size());
        PutU32(ico, offset);
        offset += (uint32_t)pngImages[i].size();
    }
    for (size_t i = 0; i < count; i++) ico.insert(ico.end(), pngImages[i].begin(), pngImages[i].end());
    return ico;
}

bool IconBuilder::BuildFromImageBytes(const std::vector<uint8_t>& bytes, const std::string& fileType, Result& out) {
    out = Result();
    if (bytes.empty()) {
        out.error = "The icon file is empty.";
        return false;
    }

    Image source = LoadImageFromMemory(fileType.c_str(), bytes.data(), (int)bytes.size());
    if (!source.data) {
        out.error = "The icon image could not be read. Use a PNG file.";
        return false;
    }
    ImageFormat(&source, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    int side = std::max(source.width, source.height);
    if (source.width < kMinSourceSize && source.height < kMinSourceSize) {
        UnloadImage(source);
        out.error = "The icon image is too small. Use at least 16x16 pixels, ideally 256x256 or larger.";
        return false;
    }

    Image square;
    if (source.width == source.height) {
        square = ImageCopy(source);
    } else {
        square = GenImageColor(side, side, BLANK);
        Rectangle from = { 0.0f, 0.0f, (float)source.width, (float)source.height };
        Rectangle to = { (float)((side - source.width) / 2), (float)((side - source.height) / 2), (float)source.width, (float)source.height };
        ImageDrawImage(&square, source, (side - source.width) / 2, (side - source.height) / 2, WHITE);
    }
    UnloadImage(source);

    std::vector<int> sizes = ChooseSizes(side);
    std::vector<std::vector<uint8_t>> images;
    for (int size : sizes) {
        Image scaled = ImageCopy(square);
        if (size != side) ImageResize(&scaled, size, size);
        std::vector<uint8_t> png;
        bool encoded = EncodePng(scaled, png);
        UnloadImage(scaled);
        if (!encoded) {
            UnloadImage(square);
            out.error = "The icon could not be converted.";
            return false;
        }
        images.push_back(std::move(png));
    }
    UnloadImage(square);

    out.png = images.back();
    out.ico = AssembleIco(images, sizes);
    out.success = true;
    return true;
}

bool IconBuilder::BuildFromFile(const std::string& path, Result& out) {
    out = Result();
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        out.error = "The icon file could not be opened: " + path;
        return false;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string extension = fs::path(path).extension().string();
    if (extension.empty()) extension = ".png";
    return BuildFromImageBytes(bytes, extension, out);
}

std::string IconBuilder::ResolveSourcePath(const std::string& projectRoot, const std::string& configuredIcon, const std::string& engineRoot) {
    std::error_code ec;
    if (!configuredIcon.empty()) {
        fs::path candidate(configuredIcon);
        if (candidate.is_relative()) candidate = fs::path(projectRoot) / candidate;
        if (fs::exists(candidate, ec)) return candidate.string();
    }
    return (fs::path(engineRoot) / "Assets" / "logo.png").string();
}
