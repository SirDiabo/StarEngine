#include "NativeFileDialog.h"
#include "Core/SafeProcess.h"
#include <cstring>
#include <cstdlib>
#include <vector>

namespace {

[[maybe_unused]] std::string BuildWindowsFilter(const std::string& filterDesc, const std::string& filterExt) {
    std::string filter = filterDesc;
    filter.push_back('\0');
    filter += filterExt;
    filter.push_back('\0');
    return filter;
}

[[maybe_unused]] std::string AppleScriptQuote(const std::string& text) {
    std::string quoted = "\"";
    for (char c : text) {
        if (c == '\\' || c == '"') {
            quoted += '\\';
            quoted += c;
        } else if (c == '\n' || c == '\r') {
            quoted += ' ';
        } else {
            quoted += c;
        }
    }
    quoted += '"';
    return quoted;
}

enum class DialogKind { OpenFile, SaveFile, OpenFolder };

[[maybe_unused]] std::vector<std::string> BuildZenityArgs(DialogKind kind, const std::string& title,
                                                          const std::string& defaultDir, const std::string& defaultName) {
    std::vector<std::string> args = { "zenity", "--file-selection" };
    if (kind == DialogKind::SaveFile) {
        args.push_back("--save");
        args.push_back("--confirm-overwrite");
    } else if (kind == DialogKind::OpenFolder) {
        args.push_back("--directory");
    }
    args.push_back("--title=" + title);
    if (kind == DialogKind::SaveFile) {
        std::string defaultPath = defaultDir.empty() ? defaultName : defaultDir + "/" + defaultName;
        if (!defaultPath.empty()) args.push_back("--filename=" + defaultPath);
    } else if (!defaultDir.empty()) {
        args.push_back("--filename=" + defaultDir + "/");
    }
    return args;
}

[[maybe_unused]] std::string BuildOsascriptSource(DialogKind kind, const std::string& title,
                                                  const std::string& defaultDir, const std::string& defaultName) {
    std::string script = "POSIX path of (";
    if (kind == DialogKind::SaveFile) {
        script += "choose file name with prompt " + AppleScriptQuote(title);
        if (!defaultName.empty()) script += " default name " + AppleScriptQuote(defaultName);
    } else if (kind == DialogKind::OpenFolder) {
        script += "choose folder with prompt " + AppleScriptQuote(title);
    } else {
        script += "choose file with prompt " + AppleScriptQuote(title);
    }
    if (!defaultDir.empty()) script += " default location (POSIX file " + AppleScriptQuote(defaultDir) + ")";
    script += ")";
    return script;
}

[[maybe_unused]] std::string RunDialogProcess(const std::vector<std::string>& argv) {
    std::string out;
    if (SafeProcess::RunCaptureStdout(argv, out) != 0) return {};
    size_t end = out.find_first_of("\r\n");
    if (end != std::string::npos) out.resize(end);
    return out;
}

} // namespace

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

static std::string WinOpenDialog(const std::string& title, const std::string& defaultDir,
                                  const std::string& filterDesc, const std::string& filterExt,
                                  bool save, const std::string& defaultName) {
    char path[MAX_PATH] = {};
    if (!defaultName.empty())
        strncpy_s(path, defaultName.c_str(), MAX_PATH - 1);

    const std::string filter = BuildWindowsFilter(filterDesc, filterExt);

    OPENFILENAMEA ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.lpstrFilter  = filter.c_str();
    ofn.lpstrFile    = path;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrTitle   = title.c_str();
    ofn.lpstrInitialDir = defaultDir.empty() ? nullptr : defaultDir.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    BOOL ok = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
    return ok ? std::string(path) : std::string();
}

static std::string WinBrowseFolder(const std::string& title, const std::string& defaultDir) {
    char path[MAX_PATH] = {};
    BROWSEINFOA bi = {};
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    (void)defaultDir; // BROWSEINFOA has no direct initial-directory field to set here.

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return {};
    SHGetPathFromIDListA(pidl, path);
    CoTaskMemFree(pidl);
    return std::string(path);
}

#endif

std::string NativeFileDialog::OpenFile(const std::string& title,
                                        const std::string& defaultDir,
                                        const std::string& filterDesc,
                                        const std::string& filterExt) {
#ifdef _WIN32
    return WinOpenDialog(title, defaultDir, filterDesc, filterExt, false, "");
#elif defined(__linux__)
    (void)filterDesc; (void)filterExt;
    return RunDialogProcess(BuildZenityArgs(DialogKind::OpenFile, title, defaultDir, ""));
#elif defined(__APPLE__)
    (void)filterDesc; (void)filterExt;
    return RunDialogProcess({ "osascript", "-e", BuildOsascriptSource(DialogKind::OpenFile, title, defaultDir, "") });
#else
    return {};
#endif
}

std::string NativeFileDialog::SaveFile(const std::string& title,
                                        const std::string& defaultDir,
                                        const std::string& defaultName,
                                        const std::string& filterDesc,
                                        const std::string& filterExt) {
#ifdef _WIN32
    return WinOpenDialog(title, defaultDir, filterDesc, filterExt, true, defaultName);
#elif defined(__linux__)
    (void)filterDesc; (void)filterExt;
    return RunDialogProcess(BuildZenityArgs(DialogKind::SaveFile, title, defaultDir, defaultName));
#elif defined(__APPLE__)
    (void)filterDesc; (void)filterExt;
    return RunDialogProcess({ "osascript", "-e", BuildOsascriptSource(DialogKind::SaveFile, title, defaultDir, defaultName) });
#else
    return {};
#endif
}

std::string NativeFileDialog::OpenFolder(const std::string& title, const std::string& defaultDir) {
#ifdef _WIN32
    return WinBrowseFolder(title, defaultDir);
#elif defined(__linux__)
    return RunDialogProcess(BuildZenityArgs(DialogKind::OpenFolder, title, defaultDir, ""));
#elif defined(__APPLE__)
    return RunDialogProcess({ "osascript", "-e", BuildOsascriptSource(DialogKind::OpenFolder, title, defaultDir, "") });
#else
    return {};
#endif
}
