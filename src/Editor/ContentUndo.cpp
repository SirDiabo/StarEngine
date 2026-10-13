#include "ContentUndo.h"
#include "Core/UndoManager.h"
#include <fstream>
#include <memory>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace ContentUndo {

bool ReadFileBytes(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

void RecordMove(const fs::path& from, const fs::path& to, const std::string& label,
                std::function<void(const fs::path&)> afterMove) {
    auto move = [](const fs::path& source, const fs::path& dest) {
        std::error_code ec;
        if (!fs::exists(source, ec) || fs::exists(dest, ec)) return false;
        fs::create_directories(dest.parent_path(), ec);
        fs::rename(source, dest, ec);
        return !ec;
    };
    auto undoFn = [move, from, to, afterMove]() {
        if (!move(to, from)) return false;
        if (afterMove) afterMove(from);
        return true;
    };
    auto redoFn = [move, from, to, afterMove]() {
        if (!move(from, to)) return false;
        if (afterMove) afterMove(to);
        return true;
    };
    UndoManager::Get().PushEntry(std::unique_ptr<UndoEntry>(new LambdaUndoEntry(label, undoFn, redoFn)));
}

void RecordFileEdit(const std::string& path, const std::string& before, const std::string& after,
                    const std::string& label, std::function<void()> afterRestore) {
    if (before == after) return;
    auto write = [path, afterRestore](const std::string& bytes) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(bytes.data(), (std::streamsize)bytes.size());
        out.close();
        if (afterRestore) afterRestore();
        return true;
    };
    auto undoFn = [write, before]() { return write(before); };
    auto redoFn = [write, after]() { return write(after); };
    UndoManager::Get().PushEntry(std::unique_ptr<UndoEntry>(
        new LambdaUndoEntry(label, undoFn, redoFn, before.size() + after.size())));
}

}
