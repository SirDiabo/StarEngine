#include "NativeFileDialog.h"
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

static std::string WinOpenDialog(const std::string& title, const std::string& defaultDir,
                                  const std::string& filterDesc, const std::string& filterExt,
                                  bool save, const std::string& defaultName) {
    char path[MAX_PATH] = {};
    if (!defaultName.empty())
        strncpy_s(path, defaultName.c_str(), MAX_PATH - 1);

    // Build null-separated filter string: "Description\0*.ext\0\0"
    char filter[256] = {};
    int pos = 0;
    for (char c : filterDesc) filter[pos++] = c;
    filter[pos++] = '\0';
    for (char c : filterExt)  filter[pos++] = c;
    filter[pos++] = '\0';
    filter[pos]   = '\0';

    OPENFILENAMEA ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.lpstrFilter  = filter;
    ofn.lpstrFile    = path;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrTitle   = title.c_str();
    ofn.lpstrInitialDir = defaultDir.empty() ? nullptr : defaultDir.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    BOOL ok = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
    return ok ? std::string(path) : std::string();
}

#elif defined(__linux__)

static std::string RunZenity(const std::string& args) {
    std::string cmd = "zenity " + args + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    char buf[4096] = {};
    if (fgets(buf, sizeof(buf), pipe)) {
        pclose(pipe);
        std::string result(buf);
        if (!result.empty() && result.back() == '\n') result.pop_back();
        return result;
    }
    pclose(pipe);
    return {};
}

#elif defined(__APPLE__)

static std::string RunOsascript(const std::string& script) {
    std::string cmd = "osascript -e '" + script + "' 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    char buf[4096] = {};
    if (fgets(buf, sizeof(buf), pipe)) {
        pclose(pipe);
        std::string result(buf);
        if (!result.empty() && result.back() == '\n') result.pop_back();
        return result;
    }
    pclose(pipe);
    return {};
}

#endif

std::string NativeFileDialog::OpenFile(const std::string& title,
                                        const std::string& defaultDir,
                                        const std::string& filterDesc,
                                        const std::string& filterExt) {
#ifdef _WIN32
    return WinOpenDialog(title, defaultDir, filterDesc, filterExt, false, "");
#elif defined(__linux__)
    std::string args = "--file-selection";
    args += " --title=\"" + title + "\"";
    if (!defaultDir.empty()) args += " --filename=\"" + defaultDir + "/\"";
    return RunZenity(args);
#elif defined(__APPLE__)
    std::string dir = defaultDir.empty() ? "~" : defaultDir;
    std::string script = "POSIX path of (choose file with prompt \"" + title + "\" default location POSIX file \"" + dir + "\")";
    return RunOsascript(script);
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
    std::string args = "--file-selection --save --confirm-overwrite";
    args += " --title=\"" + title + "\"";
    std::string defaultPath = defaultDir.empty() ? defaultName : defaultDir + "/" + defaultName;
    if (!defaultPath.empty()) args += " --filename=\"" + defaultPath + "\"";
    return RunZenity(args);
#elif defined(__APPLE__)
    std::string dir = defaultDir.empty() ? "~" : defaultDir;
    std::string script = "POSIX path of (choose file name with prompt \"" + title + "\" default name \"" + defaultName + "\" default location POSIX file \"" + dir + "\")";
    return RunOsascript(script);
#else
    return {};
#endif
}
