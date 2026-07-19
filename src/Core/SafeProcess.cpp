#include "SafeProcess.h"
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <array>
#else
#include <unistd.h>
#include <sys/wait.h>
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

int Run(const std::vector<std::string>& argv, std::string& log) {
    if (argv.empty()) return -1;

    int exitCode = -1;

#ifdef _WIN32
    std::string cmdLine;
    for (size_t i = 0; i < argv.size(); i++) {
        if (i) cmdLine += " ";
        cmdLine += "\"";
        for (char c : argv[i]) {
            if (c == '"') cmdLine += "\\\"";
            else cmdLine += c;
        }
        cmdLine += "\"";
    }

    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return -1;

    STARTUPINFOW si{ sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(), -1, nullptr, 0);
    std::wstring wideCmd(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(), -1, &wideCmd[0], wideLen);

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, &wideCmd[0], nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);
    if (!ok) {
        CloseHandle(readPipe);
        log += "Failed to start: " + argv[0] + "\n";
        return -1;
    }

    char buffer[512];
    DWORD read = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer) - 1, &read, nullptr) && read > 0) {
        buffer[read] = '\0';
        log += buffer;
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
        execvp(cargs[0], cargs.data());
        _exit(127);
    }

    close(pipeFd[1]);
    char buffer[512];
    ssize_t n = 0;
    while ((n = read(pipeFd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        log += buffer;
    }
    close(pipeFd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) exitCode = WEXITSTATUS(status);
    else exitCode = -1;
#endif

    return exitCode;
}

} // namespace SafeProcess
