#include "OSTrash.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

namespace OSTrash {

bool MoveToTrash(const std::string& path, std::string& error) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wlen <= 0) { error = "Invalid path"; return false; }
    std::wstring wpath((size_t)wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);

    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = wpath.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;

    int result = SHFileOperationW(&op);
    if (result != 0 || op.fAnyOperationsAborted) {
        error = "Recycle Bin operation failed";
        return false;
    }
    return true;
}

}

#elif defined(__APPLE__)

#include "SafeProcess.h"

namespace OSTrash {

static std::string EscapeAppleScriptString(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out;
}

bool MoveToTrash(const std::string& path, std::string& error) {
    std::string script = "tell application \"Finder\" to delete POSIX file \"" +
                          EscapeAppleScriptString(path) + "\"";
    std::vector<std::string> argv = {"osascript", "-e", script};
    std::string log;
    int code = SafeProcess::Run(argv, log);
    if (code != 0) {
        error = "Move to Trash failed: " + log;
        return false;
    }
    return true;
}

}

#else

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <ctime>

namespace fs = std::filesystem;

namespace OSTrash {

static std::string UrlEncodePath(const std::string& path) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : path) {
        bool safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '/' || c == '.' || c == '_' || c == '-';
        if (safe) out += (char)c;
        else {
            out += '%';
            out += hex[(c >> 4) & 0xF];
            out += hex[c & 0xF];
        }
    }
    return out;
}

bool MoveToTrash(const std::string& path, std::string& error) {
    const char* home = std::getenv("HOME");
    if (!home) { error = "HOME environment variable not set"; return false; }

    fs::path trashRoot = fs::path(home) / ".local" / "share" / "Trash";
    fs::path filesDir = trashRoot / "files";
    fs::path infoDir = trashRoot / "info";
    std::error_code ec;
    fs::create_directories(filesDir, ec);
    fs::create_directories(infoDir, ec);

    fs::path src(path);
    std::string stem = src.stem().string();
    std::string ext = src.extension().string();
    std::string candidateName = src.filename().string();

    fs::path destFile = filesDir / candidateName;
    fs::path destInfo = infoDir / (candidateName + ".trashinfo");

    int suffix = 1;
    while (fs::exists(destFile, ec) || fs::exists(destInfo, ec)) {
        candidateName = stem + " (" + std::to_string(suffix) + ")" + ext;
        destFile = filesDir / candidateName;
        destInfo = infoDir / (candidateName + ".trashinfo");
        suffix++;
    }

    fs::rename(src, destFile, ec);
    if (ec) {
        ec.clear();
        fs::copy(src, destFile, fs::copy_options::recursive, ec);
        if (ec) { error = "Move to Trash failed: " + ec.message(); return false; }
        fs::remove_all(src, ec);
    }

    fs::path absSrc = fs::absolute(src, ec);
    std::time_t now = std::time(nullptr);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));

    std::ofstream info(destInfo);
    info << "[Trash Info]\n";
    info << "Path=" << UrlEncodePath(absSrc.string()) << "\n";
    info << "DeletionDate=" << timeBuf << "\n";

    return true;
}

}

#endif
