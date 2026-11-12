#include "SafeProcess.h"
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace SafeProcess {

bool IsShellSafe(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        bool ok = (c >= 'A' && c <= 'Z') ||
                  (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-' ||
                  c == '/' || c == '\\' || c == ':';
        if (!ok) return false;
    }
    return true;
}

std::vector<std::string> TokenizeCommand(const std::string& cmd) {
    std::vector<std::string> args;
    std::string cur;
    bool inQuotes = false;
    for (char c : cmd) {
        if (c == '"') { inQuotes = !inQuotes; continue; }
        if (std::isspace((unsigned char)c) && !inQuotes) {
            if (!cur.empty()) { args.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) args.push_back(cur);
    return args;
}

std::string QuoteArgForWindows(const std::string& arg) {
    std::string quoted = "\"";
    size_t backslashes = 0;
    for (char c : arg) {
        if (c == '\\') {
            backslashes++;
            continue;
        }
        if (c == '"') {
            quoted.append(backslashes * 2 + 1, '\\');
            quoted += '"';
        } else {
            quoted.append(backslashes, '\\');
            quoted += c;
        }
        backslashes = 0;
    }
    quoted.append(backslashes * 2, '\\');
    quoted += '"';
    return quoted;
}

std::string BuildWindowsCommandLine(const std::vector<std::string>& argv) {
    std::string cmdLine;
    for (size_t i = 0; i < argv.size(); i++) {
        if (i) cmdLine += ' ';
        cmdLine += QuoteArgForWindows(argv[i]);
    }
    return cmdLine;
}

namespace {

#ifdef _WIN32
constexpr char kPathListSeparator = ';';
constexpr char kDirSeparator = '\\';
#else
constexpr char kPathListSeparator = ':';
constexpr char kDirSeparator = '/';
#endif

std::string GetPathVariable() {
#ifdef _WIN32
    DWORD needed = GetEnvironmentVariableA("PATH", nullptr, 0);
    if (needed == 0) return {};
    std::string value(needed, '\0');
    DWORD written = GetEnvironmentVariableA("PATH", &value[0], needed);
    value.resize(written);
    return value;
#else
    const char* value = std::getenv("PATH");
    return value ? std::string(value) : std::string();
#endif
}

bool IsExecutableFile(const std::string& path) {
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && access(path.c_str(), X_OK) == 0;
#endif
}

std::vector<std::string> SplitList(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : text) {
        if (c == separator) {
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);
    return parts;
}

std::string FindWithExtensions(const std::string& basePath, [[maybe_unused]] bool hasExtension) {
#ifdef _WIN32
    if (hasExtension && IsExecutableFile(basePath)) return basePath;
    std::string pathExt = ".COM;.EXE;.BAT;.CMD";
    DWORD needed = GetEnvironmentVariableA("PATHEXT", nullptr, 0);
    if (needed > 1) {
        std::string value(needed, '\0');
        DWORD written = GetEnvironmentVariableA("PATHEXT", &value[0], needed);
        value.resize(written);
        pathExt = value;
    }
    for (const std::string& ext : SplitList(pathExt, ';')) {
        if (ext.empty()) continue;
        std::string candidate = basePath + ext;
        if (IsExecutableFile(candidate)) return candidate;
    }
    return {};
#else
    return IsExecutableFile(basePath) ? basePath : std::string();
#endif
}

} // namespace

std::string FindExecutableInPath(const std::string& name) {
    if (name.empty()) return {};

    const size_t lastSeparator = name.find_last_of("/\\");
    const std::string lastComponent = lastSeparator == std::string::npos ? name : name.substr(lastSeparator + 1);
    const bool hasExtension = lastComponent.find('.') != std::string::npos;

    if (lastSeparator != std::string::npos) return FindWithExtensions(name, hasExtension);

    for (std::string dir : SplitList(GetPathVariable(), kPathListSeparator)) {
        if (dir.size() >= 2 && dir.front() == '"' && dir.back() == '"') dir = dir.substr(1, dir.size() - 2);
        if (dir.empty()) continue;
        if (dir.back() != kDirSeparator && dir.back() != '/') dir += kDirSeparator;
        std::string found = FindWithExtensions(dir + name, hasExtension);
        if (!found.empty()) return found;
    }
    return {};
}

namespace {

int RunImpl(const std::vector<std::string>& argv, std::string& log,
            const std::function<void(const std::string&)>& onChunk, bool discardStderr) {
    if (argv.empty()) return -1;

    int exitCode = -1;

#ifdef _WIN32
    const std::string cmdLine = BuildWindowsCommandLine(argv);

    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return -1;

    HANDLE nullDevice = INVALID_HANDLE_VALUE;
    if (discardStderr) {
        nullDevice = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    STARTUPINFOW si{ sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = nullDevice != INVALID_HANDLE_VALUE ? nullDevice : writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(), -1, nullptr, 0);
    std::wstring wideCmd(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(), -1, &wideCmd[0], wideLen);

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, &wideCmd[0], nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);
    if (nullDevice != INVALID_HANDLE_VALUE) CloseHandle(nullDevice);
    if (!ok) {
        CloseHandle(readPipe);
        log += "Failed to start: " + argv[0] + "\n";
        return -1;
    }

    char buffer[512];
    DWORD read = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        log.append(buffer, read);
        if (onChunk) onChunk(std::string(buffer, read));
    }
    CloseHandle(readPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    exitCode = (int)code;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    std::vector<char*> cargs;
    for (const auto& a : argv) {
        cargs.push_back(const_cast<char*>(a.c_str()));
    }
    cargs.push_back(nullptr);

    int pipeFd[2];
    if (pipe(pipeFd) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipeFd[0]);
        close(pipeFd[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(pipeFd[1], STDOUT_FILENO);
        dup2(pipeFd[1], STDERR_FILENO);
        close(pipeFd[0]);
        close(pipeFd[1]);
        if (discardStderr) {
            int nullFd = open("/dev/null", O_WRONLY);
            if (nullFd >= 0) {
                dup2(nullFd, STDERR_FILENO);
                close(nullFd);
            }
        }
        execvp(cargs[0], cargs.data());
        if (!discardStderr) perror(cargs[0]);
        _exit(127);
    }

    close(pipeFd[1]);
    char buffer[512];
    for (;;) {
        ssize_t n = read(pipeFd[0], buffer, sizeof(buffer));
        if (n > 0) {
            log.append(buffer, (size_t)n);
            if (onChunk) onChunk(std::string(buffer, (size_t)n));
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        break;
    }
    close(pipeFd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) exitCode = WEXITSTATUS(status);
    else exitCode = -1;
#endif

    return exitCode;
}

} // namespace

int Run(const std::vector<std::string>& argv, std::string& log,
        const std::function<void(const std::string&)>& onChunk) {
    return RunImpl(argv, log, onChunk, false);
}

int RunCaptureStdout(const std::vector<std::string>& argv, std::string& out) {
    return RunImpl(argv, out, nullptr, true);
}

} // namespace SafeProcess
