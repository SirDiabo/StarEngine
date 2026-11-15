#include "TestHelper.h"
#include "TestFiles.h"
#include "Core/PathSafety.h"
#include "Core/SafeProcess.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

namespace fs = std::filesystem;
using test::TempDir;

std::string g_selfPath;

bool SameFile(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    return fs::equivalent(a, b, ec) && !ec;
}

bool MakeSymlink(const fs::path& target, const fs::path& link, bool directory) {
    std::error_code ec;
    if (directory) fs::create_directory_symlink(target, link, ec);
    else fs::create_symlink(target, link, ec);
    return !ec;
}

void SetEnv(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void UnsetEnv(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

class ScopedPathVariable {
public:
    explicit ScopedPathVariable(const std::string& value) {
        const char* current = std::getenv("PATH");
        hadValue = current != nullptr;
        if (hadValue) previous = current;
        SetEnv("PATH", value);
    }
    ~ScopedPathVariable() {
        if (hadValue) SetEnv("PATH", previous);
        else UnsetEnv("PATH");
    }
    ScopedPathVariable(const ScopedPathVariable&) = delete;
    ScopedPathVariable& operator=(const ScopedPathVariable&) = delete;

private:
    bool hadValue = false;
    std::string previous;
};

std::vector<std::string> ParseWindowsCommandLine(const std::string& line) {
    std::vector<std::string> args;
    size_t i = 0;
    const size_t n = line.size();
    while (true) {
        while (i < n && (line[i] == ' ' || line[i] == '\t')) i++;
        if (i >= n) break;
        std::string arg;
        bool inQuote = false;
        while (i < n) {
            char c = line[i];
            if ((c == ' ' || c == '\t') && !inQuote) break;
            if (c == '\\') {
                size_t slashes = 0;
                while (i < n && line[i] == '\\') {
                    slashes++;
                    i++;
                }
                if (i < n && line[i] == '"') {
                    arg.append(slashes / 2, '\\');
                    if (slashes % 2 == 1) {
                        arg += '"';
                        i++;
                    }
                } else {
                    arg.append(slashes, '\\');
                }
                continue;
            }
            if (c == '"') {
                if (inQuote && i + 1 < n && line[i + 1] == '"') {
                    arg += '"';
                    i += 2;
                    continue;
                }
                inQuote = !inQuote;
                i++;
                continue;
            }
            arg += c;
            i++;
        }
        args.push_back(arg);
    }
    return args;
}

std::string HexOf(const std::string& text) {
    return test::Hex(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

std::string FromHexString(const std::string& hex) {
    std::vector<uint8_t> bytes = test::FromHex(hex);
    return std::string(bytes.begin(), bytes.end());
}

int RunChildMode(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif
    std::string mode = argv[1];
    if (mode == "--child-echo") {
        for (int i = 2; i < argc; i++) std::printf("%d:%s\n", i - 2, HexOf(argv[i]).c_str());
        std::fflush(stdout);
        return 0;
    }
    if (mode == "--child-exit" && argc > 2) {
        return std::atoi(argv[2]);
    }
    if (mode == "--child-streams") {
        std::fputs("OUT", stdout);
        std::fflush(stdout);
        std::fputs("ERR", stderr);
        std::fflush(stderr);
        return 0;
    }
    if (mode == "--child-flood" && argc > 2) {
        long total = std::atol(argv[2]);
        std::string chunk(1000, 'x');
        for (long written = 0; written < total; written += 1000) {
            std::fwrite(chunk.data(), 1, 1000, stdout);
            std::fwrite(chunk.data(), 1, 500, stderr);
        }
        std::fflush(stdout);
        std::fflush(stderr);
        return 3;
    }
    return 99;
}

std::vector<std::string> SelfCommand(std::initializer_list<std::string> extra) {
    std::vector<std::string> argv = { g_selfPath };
    argv.insert(argv.end(), extra.begin(), extra.end());
    return argv;
}

std::vector<std::string> ParseEchoOutput(const std::string& log, size_t expectedCount, bool& ok) {
    std::vector<std::string> args(expectedCount);
    ok = true;
    size_t pos = 0;
    size_t seen = 0;
    while (pos < log.size()) {
        size_t nl = log.find('\n', pos);
        if (nl == std::string::npos) nl = log.size();
        std::string line = log.substr(pos, nl - pos);
        pos = nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t colon = line.find(':');
        if (colon == std::string::npos) { ok = false; continue; }
        size_t index = (size_t)std::atoi(line.substr(0, colon).c_str());
        if (index >= expectedCount) { ok = false; continue; }
        args[index] = FromHexString(line.substr(colon + 1));
        seen++;
    }
    if (seen != expectedCount) ok = false;
    return args;
}

}

TEST(plain_file_name_rules) {
    for (const char* good : { "a.png", "tex.png", "UPPER.PNG", "name with spaces.png", "dots.in.name.png", "...", "-leading", "tilde~", "unicode\xC3\xBC.png", "a" }) {
        CHECK_MSG(PathSafety::IsPlainFileName(good), good);
    }
    CHECK(PathSafety::IsPlainFileName(std::string(255, 'a')));

    CHECK(!PathSafety::IsPlainFileName(""));
    CHECK(!PathSafety::IsPlainFileName("."));
    CHECK(!PathSafety::IsPlainFileName(".."));
    CHECK(!PathSafety::IsPlainFileName(std::string(256, 'a')));
    for (const char* bad : { "a/b", "/a", "a/", "a\\b", "\\a", "C:x", "C:", "a:b", "a\nb", "a\tb", "a\rb", "\x01" "a", "a\x1f" }) {
        CHECK_MSG(!PathSafety::IsPlainFileName(bad), std::string("rejected: ") + bad);
    }
    CHECK(!PathSafety::IsPlainFileName(std::string("a\0b", 3)));
    CHECK(!PathSafety::IsPlainFileName(std::string("\0", 1)));
}

TEST(extension_classification) {
    for (const char* image : { "a.png", "a.PNG", "a.Jpg", "a.jpeg", "a.bmp", "a.tga", "a.gif", "a.psd", "a.hdr", "a.qoi", "a.dds", "dir/sub/a.png" }) {
        CHECK_MSG(PathSafety::IsImageExtension(image), image);
    }
    for (const char* notImage : { "a.txt", "a", "a.", "a.png.txt", ".png", "png", "a.obj", "a.mtl", "" }) {
        CHECK_MSG(!PathSafety::IsImageExtension(notImage), notImage);
    }
    CHECK(PathSafety::IsMtlExtension("m.mtl"));
    CHECK(PathSafety::IsMtlExtension("M.MTL"));
    CHECK(!PathSafety::IsMtlExtension("m.obj"));
    CHECK(!PathSafety::IsMtlExtension("m"));

    for (const char* media : { "a.obj", "a.gltf", "a.glb", "a.iqm", "a.wav", "a.mp3", "a.ogg", "a.flac", "a.xm", "a.mod", "a.ttf", "a.otf", "a.mid", "a.midi", "a.sf2", "a.png", "a.mtl", "A.OGG" }) {
        CHECK_MSG(PathSafety::IsMediaSourceExtension(media), media);
    }
    for (const char* notMedia : { "a.txt", "a.exe", "a.dll", "a.starmap", "a.cpp", "a", "a.zip" }) {
        CHECK_MSG(!PathSafety::IsMediaSourceExtension(notMedia), notMedia);
    }
}

TEST(inside_accepts_paths_within_root) {
    TempDir dir;
    fs::path root = dir.Path("root");
    REQUIRE(test::WriteText(root / "file.txt", "x"));
    REQUIRE(test::WriteText(root / "a" / "b" / "deep.txt", "x"));

    CHECK(PathSafety::IsInside(root, root));
    CHECK(PathSafety::IsInside(root, root / "file.txt"));
    CHECK(PathSafety::IsInside(root, root / "a" / "b" / "deep.txt"));
    CHECK(PathSafety::IsInside(root, root / "does" / "not" / "exist.txt"));
    CHECK(PathSafety::IsInside(root, root / "a" / ".." / "file.txt"));
    CHECK(PathSafety::IsInside(root, root / ".." / "root" / "file.txt"));
    CHECK(PathSafety::IsInside(root / "a", root / "a" / "b" / "deep.txt"));
    CHECK(PathSafety::IsInside(root.string() + "/", root / "file.txt"));
}

TEST(inside_rejects_paths_outside_root) {
    TempDir dir;
    fs::path root = dir.Path("root");
    REQUIRE(test::WriteText(root / "file.txt", "x"));
    REQUIRE(test::WriteText(dir.Path("root2") / "file.txt", "x"));
    REQUIRE(test::WriteText(dir.Path("outside.txt"), "x"));

    CHECK(!PathSafety::IsInside(root, dir.path));
    CHECK(!PathSafety::IsInside(root, dir.Path("outside.txt")));
    CHECK(!PathSafety::IsInside(root, root / ".." / "outside.txt"));
    CHECK(!PathSafety::IsInside(root, root / "a" / ".." / ".." / "outside.txt"));
    CHECK(!PathSafety::IsInside(root, dir.Path("root2") / "file.txt"));
    CHECK(!PathSafety::IsInside(root, root.parent_path()));
    CHECK(!PathSafety::IsInside(root / "a", root / "file.txt"));
    CHECK(!PathSafety::IsInside(root / "a", root));
}

TEST(inside_follows_symlinks_out_of_the_root) {
    TempDir dir;
    fs::path root = dir.Path("root");
    fs::path elsewhere = dir.Path("elsewhere");
    REQUIRE(test::WriteText(root / "inside.txt", "x"));
    REQUIRE(test::WriteText(elsewhere / "secret.txt", "x"));

    if (!MakeSymlink(elsewhere, root / "link_dir", true)) {
        std::printf("    skipped: symbolic links are not available here\n");
        return;
    }
    CHECK(!PathSafety::IsInside(root, root / "link_dir"));
    CHECK(!PathSafety::IsInside(root, root / "link_dir" / "secret.txt"));
    CHECK(!PathSafety::IsInside(root, root / "link_dir" / "new_file.txt"));

    REQUIRE(MakeSymlink(root / "inside.txt", root / "link_in.txt", false));
    CHECK(PathSafety::IsInside(root, root / "link_in.txt"));
}

TEST(resolve_references_inside_the_base_directory) {
    TempDir dir;
    fs::path base = dir.Path("assets");
    REQUIRE(test::WriteText(base / "tex.png", "x"));
    REQUIRE(test::WriteText(base / "sub" / "tex2.png", "x"));
    REQUIRE(test::WriteText(base / "mat.mtl", "x"));
    REQUIRE(test::WriteText(base / "UPPER.PNG", "x"));
    REQUIRE(test::WriteText(base / "notes.txt", "x"));
    REQUIRE(test::WriteText(dir.Path("outside.png"), "x"));
    REQUIRE(test::WriteText(dir.Path("outside.mtl"), "x"));

    auto resolves = [&](const std::string& reference, const fs::path& expected) {
        fs::path result = PathSafety::ResolveTextureReference(base, reference);
        return !result.empty() && SameFile(result, expected);
    };

    CHECK(resolves("tex.png", base / "tex.png"));
    CHECK(resolves("./tex.png", base / "tex.png"));
    CHECK(resolves("sub/tex2.png", base / "sub" / "tex2.png"));
    CHECK(resolves("sub\\tex2.png", base / "sub" / "tex2.png"));
    CHECK(resolves("sub/../tex.png", base / "tex.png"));
    CHECK(resolves("../assets/tex.png", base / "tex.png"));
    CHECK(resolves("UPPER.PNG", base / "UPPER.PNG"));
    CHECK(resolves("C:\\Users\\someone\\Pictures\\tex.png", base / "tex.png"));
    CHECK(resolves("/usr/share/pictures/tex.png", base / "tex.png"));
    CHECK(PathSafety::ResolveTextureReference(base, "elsewhere/in/the/tree/tex2.png").empty());

    for (const char* reference : { "../outside.png", "sub/../../outside.png", "..\\outside.png", "/outside.png", "notes.txt", "tex.png.txt", "tex", "missing.png", "sub", "mat.mtl" }) {
        CHECK_MSG(PathSafety::ResolveTextureReference(base, reference).empty(), reference);
    }

    CHECK(PathSafety::ResolveTextureReference(base, "").empty());
    CHECK(PathSafety::ResolveTextureReference(base, std::string(1025, 'a') + ".png").empty());
    CHECK(PathSafety::ResolveTextureReference(base, std::string("tex.png\0", 8)).empty());
    CHECK(PathSafety::ResolveTextureReference(base, std::string("te\0x.png", 8)).empty());

    fs::path mtl = PathSafety::ResolveMtlReference(base, "mat.mtl");
    CHECK(!mtl.empty() && SameFile(mtl, base / "mat.mtl"));
    CHECK(PathSafety::ResolveMtlReference(base, "../outside.mtl").empty());
    CHECK(PathSafety::ResolveMtlReference(base, "tex.png").empty());
    CHECK(PathSafety::ResolveMtlReference(base, "").empty());
}

TEST(resolve_references_skip_directories_and_device_names) {
    TempDir dir;
    fs::path base = dir.Path("assets");
    std::error_code ec;
    fs::create_directories(base / "folder.png", ec);
    REQUIRE(test::WriteText(base / "tex.png", "x"));

    CHECK(PathSafety::ResolveTextureReference(base, "folder.png").empty());
    for (const char* name : { "CON.png", "NUL.png", "AUX.png", "COM1.png", "LPT1.png", "PRN.png" }) {
        CHECK_MSG(PathSafety::ResolveTextureReference(base, name).empty(), name);
    }
    CHECK(PathSafety::ResolveTextureReference(base, "tex.png:evil.png").empty());
    CHECK(PathSafety::ResolveTextureReference(base, "tex.png::$DATA").empty());
}

TEST(resolve_references_refuse_symlinks_that_leave_the_base) {
    TempDir dir;
    fs::path base = dir.Path("assets");
    fs::path elsewhere = dir.Path("elsewhere");
    REQUIRE(test::WriteText(base / "tex.png", "x"));
    REQUIRE(test::WriteText(dir.Path("outside.png"), "x"));
    REQUIRE(test::WriteText(elsewhere / "tex3.png", "x"));

    if (!MakeSymlink(dir.Path("outside.png"), base / "link_out.png", false)) {
        std::printf("    skipped: symbolic links are not available here\n");
        return;
    }
    REQUIRE(MakeSymlink(elsewhere, base / "link_dir", true));

    CHECK(PathSafety::ResolveTextureReference(base, "link_out.png").empty());
    CHECK(PathSafety::ResolveTextureReference(base, "link_dir/tex3.png").empty());
    CHECK(!PathSafety::ResolveTextureReference(base, "tex.png").empty());
}

TEST(shell_safe_characters) {
    for (const char* safe : { "abc", "ABC", "a1", "file.txt", "a_b", "a-b", "path/to/file", "C:\\dir\\file.exe", "/usr/bin/gcc" }) {
        CHECK_MSG(SafeProcess::IsShellSafe(safe), safe);
    }
    CHECK(!SafeProcess::IsShellSafe(""));
    for (const char* unsafe : { "a b", "a;b", "a|b", "a&b", "a$b", "a`b", "a\"b", "a'b", "a\nb", "a\tb", "a*b", "a?b", "a<b", "a>b", "a(b", "a)b", "a%b", "a!b", "a^b", "a~b", "a#b", "a=b", "a,b", "a{b", "a[b", "a\\\"b", "g++" }) {
        CHECK_MSG(!SafeProcess::IsShellSafe(unsafe), unsafe);
    }
    CHECK(!SafeProcess::IsShellSafe(std::string("a\0b", 3)));
    CHECK(!SafeProcess::IsShellSafe("\xC3\xBC"));
}

TEST(tokenize_command_rules) {
    using V = std::vector<std::string>;
    CHECK(SafeProcess::TokenizeCommand("") == V());
    CHECK(SafeProcess::TokenizeCommand("   \t  ") == V());
    CHECK(SafeProcess::TokenizeCommand("a") == V({ "a" }));
    CHECK(SafeProcess::TokenizeCommand("a b  c\td") == V({ "a", "b", "c", "d" }));
    CHECK(SafeProcess::TokenizeCommand("  lead and trail  ") == V({ "lead", "and", "trail" }));
    CHECK(SafeProcess::TokenizeCommand("a \"b c\" d") == V({ "a", "b c", "d" }));
    CHECK(SafeProcess::TokenizeCommand("\"quoted program\" arg") == V({ "quoted program", "arg" }));
    CHECK(SafeProcess::TokenizeCommand("a\"b c\"d") == V({ "ab cd" }));
    CHECK(SafeProcess::TokenizeCommand("a \"unterminated rest of line") == V({ "a", "unterminated rest of line" }));
    CHECK(SafeProcess::TokenizeCommand("C:\\Program Files\\x") == V({ "C:\\Program", "Files\\x" }));
    CHECK(SafeProcess::TokenizeCommand("\"C:\\Program Files\\x\" --flag") == V({ "C:\\Program Files\\x", "--flag" }));
    CHECK(SafeProcess::TokenizeCommand("a;b|c&d") == V({ "a;b|c&d" }));
}

TEST(windows_quoting_known_answers) {
    struct Case { std::string input; std::string expected; };
    std::vector<Case> cases = {
        { "abc", "\"abc\"" },
        { "", "\"\"" },
        { "a b", "\"a b\"" },
        { "a\tb", "\"a\tb\"" },
        { "a\"b", "\"a\\\"b\"" },
        { "\"", "\"\\\"\"" },
        { "\"\"", "\"\\\"\\\"\"" },
        { "a\\b", "\"a\\b\"" },
        { "a\\\\b", "\"a\\\\b\"" },
        { "a\\\"b", "\"a\\\\\\\"b\"" },
        { "a\\\\\"b", "\"a\\\\\\\\\\\"b\"" },
        { "a\\", "\"a\\\\\"" },
        { "a\\\\", "\"a\\\\\\\\\"" },
        { "\\", "\"\\\\\"" },
        { "C:\\Program Files\\App\\", "\"C:\\Program Files\\App\\\\\"" },
        { "C:\\dir\\file.exe", "\"C:\\dir\\file.exe\"" },
        { "--flag=\"value with space\"", "\"--flag=\\\"value with space\\\"\"" },
    };
    for (const Case& c : cases) {
        CHECK_EQ_STR(SafeProcess::QuoteArgForWindows(c.input), c.expected);
    }
    CHECK_EQ_STR(SafeProcess::BuildWindowsCommandLine({ "prog", "a b", "" }), "\"prog\" \"a b\" \"\"");
    CHECK_EQ_STR(SafeProcess::BuildWindowsCommandLine({}), "");
    CHECK_EQ_STR(SafeProcess::BuildWindowsCommandLine({ "only" }), "\"only\"");
}

TEST(windows_quoting_round_trips_exhaustively) {
    const char alphabet[] = { 'a', ' ', '\\', '"', '\t' };
    std::vector<std::string> inputs = { "" };
    size_t frontier = 0;
    for (int length = 1; length <= 7; length++) {
        size_t end = inputs.size();
        for (size_t i = frontier; i < end; i++) {
            if (inputs[i].size() != (size_t)(length - 1)) continue;
            for (char c : alphabet) inputs.push_back(inputs[i] + c);
        }
        frontier = end;
    }
    CHECK(inputs.size() > 90000);

    for (const std::string& input : inputs) {
        std::string quoted = SafeProcess::QuoteArgForWindows(input);
        std::vector<std::string> parsed = ParseWindowsCommandLine(quoted);
        if (parsed.size() != 1 || parsed[0] != input) {
            CHECK_MSG(false, "round trip failed for [" + HexOf(input) + "] quoted as [" + quoted + "]");
            break;
        }
    }
}

TEST(windows_command_line_round_trips_argument_lists) {
    test::Rng rng(55);
    const char alphabet[] = { 'a', 'b', ' ', '\\', '"', '\t', '=', '-', '/' };
    for (int round = 0; round < 20000; round++) {
        std::vector<std::string> argv;
        int count = 1 + (int)rng.Below(5);
        for (int i = 0; i < count; i++) {
            std::string arg;
            int length = (int)rng.Below(9);
            for (int j = 0; j < length; j++) arg += alphabet[rng.Below(9)];
            argv.push_back(arg);
        }
        std::vector<std::string> parsed = ParseWindowsCommandLine(SafeProcess::BuildWindowsCommandLine(argv));
        if (parsed != argv) {
            CHECK_MSG(false, "argument list round trip failed: " + SafeProcess::BuildWindowsCommandLine(argv));
            break;
        }
    }
}

TEST(find_executable_in_path) {
    TempDir dir;
    fs::path bin = dir.Path("bin");
    fs::path other = dir.Path("other");
    std::error_code ec;
    fs::create_directories(other, ec);
#ifdef _WIN32
    REQUIRE(test::WriteText(bin / "mytool.exe", "x"));
    REQUIRE(test::WriteText(bin / "script.cmd", "x"));
    REQUIRE(test::WriteText(bin / "notatool.txt", "x"));
    const char separator = ';';
#else
    REQUIRE(test::WriteText(bin / "mytool", "x"));
    fs::permissions(bin / "mytool", fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec, ec);
    REQUIRE(test::WriteText(bin / "notexec", "x"));
    fs::permissions(bin / "notexec", fs::perms::owner_read | fs::perms::owner_write, ec);
    const char separator = ':';
    fs::create_directories(bin / "adir", ec);
#endif

    {
        ScopedPathVariable scoped(other.string() + separator + bin.string());
        std::string found = SafeProcess::FindExecutableInPath("mytool");
        CHECK(!found.empty());
        CHECK(found.find("mytool") != std::string::npos);
        CHECK(SameFile(found, bin / (found.find(".exe") != std::string::npos ? "mytool.exe" : "mytool")));

        CHECK(SafeProcess::FindExecutableInPath("definitely_not_a_real_tool").empty());
        CHECK(SafeProcess::FindExecutableInPath("").empty());
#ifdef _WIN32
        CHECK(!SafeProcess::FindExecutableInPath("script").empty());
        CHECK(!SafeProcess::FindExecutableInPath("script.cmd").empty());
        CHECK(SafeProcess::FindExecutableInPath("notatool").empty());
#else
        CHECK(SafeProcess::FindExecutableInPath("notexec").empty());
        CHECK(SafeProcess::FindExecutableInPath("adir").empty());
#endif
#ifdef _WIN32
        CHECK(!SafeProcess::FindExecutableInPath((bin / "mytool.exe").string()).empty());
        CHECK(SafeProcess::FindExecutableInPath((bin / "missing.exe").string()).empty());
#else
        CHECK(SafeProcess::FindExecutableInPath((bin / "mytool").string()) == (bin / "mytool").string());
        CHECK(SafeProcess::FindExecutableInPath((bin / "missing").string()).empty());
#endif
    }

    {
        ScopedPathVariable scoped(other.string());
        CHECK(SafeProcess::FindExecutableInPath("mytool").empty());
    }
    {
        ScopedPathVariable scoped("");
        CHECK(SafeProcess::FindExecutableInPath("mytool").empty());
    }
}

TEST(run_reports_exit_codes) {
    std::string log;
    CHECK(SafeProcess::Run(SelfCommand({ "--child-exit", "0" }), log) == 0);
    CHECK(SafeProcess::Run(SelfCommand({ "--child-exit", "7" }), log) == 7);
    CHECK(SafeProcess::Run(SelfCommand({ "--child-exit", "255" }), log) == 255);
    CHECK(SafeProcess::Run({}, log) == -1);
}

TEST(run_reports_failure_to_start) {
    std::string log;
    int code = SafeProcess::Run({ "this_program_does_not_exist_for_starengine_tests" }, log);
    CHECK(code != 0);
    CHECK(!log.empty());

    std::string out;
    CHECK(SafeProcess::RunCaptureStdout({ "this_program_does_not_exist_for_starengine_tests" }, out) != 0);
}

TEST(run_merges_stdout_and_stderr_but_capture_keeps_only_stdout) {
    std::string log;
    CHECK(SafeProcess::Run(SelfCommand({ "--child-streams" }), log) == 0);
    CHECK(log.find("OUT") != std::string::npos);
    CHECK(log.find("ERR") != std::string::npos);

    std::string captured;
    CHECK(SafeProcess::RunCaptureStdout(SelfCommand({ "--child-streams" }), captured) == 0);
    CHECK_EQ_STR(captured, "OUT");
}

TEST(run_streams_chunks_and_survives_large_output) {
    std::string log;
    std::string streamed;
    int chunks = 0;
    int code = SafeProcess::Run(SelfCommand({ "--child-flood", "300000" }), log, [&](const std::string& chunk) {
        streamed += chunk;
        chunks++;
    });
    CHECK(code == 3);
    CHECK(log.size() >= 300000);
    CHECK(streamed == log);
    CHECK(chunks > 1);
}

TEST(run_passes_arguments_exactly) {
    std::vector<std::vector<std::string>> cases = {
        { "plain" },
        { "with space", "another one" },
        { "" },
        { "", "", "x", "" },
        { "quote\"inside" },
        { "\"" },
        { "\"\"" },
        { "back\\slash" },
        { "trailing\\" },
        { "trailing\\\\" },
        { "mid\\\"quote" },
        { "C:\\Program Files\\App\\" },
        { "--flag=\"value with space\"" },
        { "$HOME", "`whoami`", "$(id)", "%PATH%", "!x!" },
        { "semi;colon", "pipe|char", "amp&ersand", "gt>file", "lt<file" },
        { "glob*", "q?", "[a-z]", "~", "{a,b}" },
        { "tab\there" },
        { "new\nline" },
        { "#hash", "=equals", "'single'", "(paren)" },
    };
    for (const auto& args : cases) {
        std::vector<std::string> argv = { g_selfPath, "--child-echo" };
        argv.insert(argv.end(), args.begin(), args.end());
        std::string log;
        int code = SafeProcess::Run(argv, log);
        CHECK(code == 0);
        bool ok = false;
        std::vector<std::string> received = ParseEchoOutput(log, args.size(), ok);
        CHECK_MSG(ok, "child output could be parsed for first argument [" + HexOf(args.empty() ? "" : args[0]) + "]");
        CHECK_MSG(received == args, "arguments differ for first argument [" + HexOf(args.empty() ? "" : args[0]) + "]");
    }
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strncmp(argv[1], "--child-", 8) == 0) return RunChildMode(argc, argv);

    fs::path self = argv[0];
    if (self.string().find_first_of("/\\") == std::string::npos) {
        std::string found = SafeProcess::FindExecutableInPath(self.string());
        if (!found.empty()) self = found;
    }
    std::error_code ec;
    self = fs::absolute(self, ec);
    g_selfPath = self.string();
    return ::test::RunAll(argc, argv);
}
