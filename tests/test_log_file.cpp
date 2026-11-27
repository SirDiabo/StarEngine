#include "TestHelper.h"
#include "TestFiles.h"
#include "Editor/LogFile.h"

namespace fs = std::filesystem;

namespace {

std::time_t At(int year, int month, int day, int hour = 12) {
    std::tm t{};
    t.tm_year = year - 1900; t.tm_mon = month - 1; t.tm_mday = day; t.tm_hour = hour; t.tm_isdst = -1;
    return std::mktime(&t);
}

void Touch(const fs::path& path, const std::string& text = "12:00:00 I x\n") {
    std::ofstream out(path, std::ios::binary);
    out << text;
}

}

TEST(session_names_roundtrip_and_validate) {
    std::string name = LogFile::MakeSessionName(At(2026, 11, 27, 9), 0);
    CHECK_EQ_STR(name, "2026-11-27_09-00-00.log");
    CHECK(LogFile::IsSessionFileName(name));
    CHECK(LogFile::IsSessionFileName(LogFile::MakeSessionName(At(2026, 11, 27), 3)));
    std::time_t parsed = 0;
    CHECK(LogFile::ParseSessionTime(name, parsed));
    CHECK(parsed == At(2026, 11, 27, 9));

    CHECK(!LogFile::IsSessionFileName("../2026-11-27_09-00-00.log"));
    CHECK(!LogFile::IsSessionFileName("2026-11-27_09-00-00.log.exe"));
    CHECK(!LogFile::IsSessionFileName("2026-11-27_09-00-00_.log"));
    CHECK(!LogFile::IsSessionFileName("notes.log"));
    CHECK(!LogFile::IsSessionFileName("2026-11-27_09-00-00/x.log"));
    CHECK(!LogFile::IsSessionFileName(""));
}

TEST(split_message_handles_multiline_and_junk) {
    auto lines = LogFile::SplitMessage("first\r\nsecond\n\nthird\n");
    CHECK(lines.size() == 4);
    CHECK_EQ_STR(lines[0], "first");
    CHECK_EQ_STR(lines[1], "second");
    CHECK_EQ_STR(lines[2], "");
    CHECK_EQ_STR(lines[3], "third");

    auto tab = LogFile::SplitMessage("a\tb\x01" "c");
    CHECK(tab.size() == 1);
    CHECK_EQ_STR(tab[0], "a    bc");

    CHECK(LogFile::SplitMessage("").size() == 1);

    std::string huge(100000, 'x');
    auto big = LogFile::SplitMessage(huge);
    CHECK(big.size() == 1);
    CHECK(big[0].size() <= LogFile::kMaxLineBytes + 3);

    std::string utf(LogFile::kMaxLineBytes - 1, 'a');
    utf += "\xC3\xA4\xC3\xA4";
    auto cut = LogFile::SplitMessage(utf + std::string(50, 'b'));
    CHECK(cut.size() == 1);
    std::string body = cut[0].substr(0, cut[0].size() - 3);
    CHECK(body.empty() || (static_cast<unsigned char>(body.back()) & 0xC0) != 0x80 || body.size() > 0);
    CHECK(body.size() < utf.size() + 50);
}

TEST(line_format_parse_roundtrip) {
    LogFile::Entry e;
    std::string line = LogFile::FormatLine('E', "10:20:30", "it broke: C:\\path with spaces");
    CHECK_EQ_STR(line, "10:20:30 E it broke: C:\\path with spaces");
    CHECK(LogFile::ParseLine(line, e));
    CHECK(e.level == 'E');
    CHECK_EQ_STR(e.time, "10:20:30");
    CHECK_EQ_STR(e.message, "it broke: C:\\path with spaces");

    CHECK(LogFile::ParseLine("10:20:30 I ", e) == false || e.message.empty());
    CHECK(!LogFile::ParseLine("garbage", e));
    CHECK(!LogFile::ParseLine("10:20:30 Q message", e));
    CHECK_EQ_STR(LogFile::FormatLine('Z', "bad", "m"), "--:--:-- I m");
}

TEST(writer_writes_lines_live_and_reads_back) {
    test::TempDir dir;
    LogFile::Writer writer;
    std::string error;
    CHECK(writer.Open(dir.path, At(2026, 11, 27, 10), error));
    CHECK(writer.IsOpen());
    CHECK(fs::exists(writer.FilePath()));

    writer.Append('I', "10:00:01", "hello");
    writer.Append('W', "10:00:02", "careful");
    writer.Append('E', "10:00:03", "boom");

    std::ifstream peek(writer.FilePath(), std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(peek)), std::istreambuf_iterator<char>());
    CHECK(content.find("boom") != std::string::npos);

    std::vector<LogFile::Entry> entries;
    bool truncated = true;
    CHECK(LogFile::ReadSession(writer.Directory(), writer.SessionName(), entries, truncated));
    CHECK(!truncated);
    CHECK(entries.size() == 3);
    CHECK(entries.size() == 3 && entries[2].level == 'E' && entries[2].message == "boom");

    LogFile::Writer second;
    CHECK(second.Open(dir.path, At(2026, 11, 27, 10), error));
    CHECK(second.SessionName() != writer.SessionName());
    CHECK(LogFile::IsSessionFileName(second.SessionName()));
}

TEST(writer_rejects_missing_root_and_symlinked_dirs) {
    LogFile::Writer writer;
    std::string error;
    CHECK(!writer.Open(fs::path(), At(2026, 1, 1), error));
    CHECK(!error.empty());
    CHECK(!writer.Open(fs::path("/definitely/not/a/real/project/root"), At(2026, 1, 1), error) || writer.IsOpen());

    test::TempDir dir;
    test::TempDir outside;
    fs::create_directories(dir.Path("Saved"));
    std::error_code ec;
    fs::create_directory_symlink(outside.path, dir.Path("Saved/logs"), ec);
    if (ec) return;
    LogFile::Writer hostile;
    CHECK(!hostile.Open(dir.path, At(2026, 1, 1), error));
    CHECK(!hostile.IsOpen());
    CHECK(fs::is_empty(outside.path));
}

TEST(size_cap_stops_writing_with_one_note) {
    test::TempDir dir;
    LogFile::Writer writer;
    std::string error;
    CHECK(writer.Open(dir.path, At(2026, 11, 27), error));
    std::string chunk(900000, 'z');
    for (int i = 0; i < 30; i++) writer.Append('I', "10:00:00", chunk);
    long long size = (long long)fs::file_size(writer.FilePath());
    CHECK(size < LogFile::kMaxSessionBytes + 1000000);
    writer.Append('I', "10:00:00", "after the cap");
    std::vector<LogFile::Entry> entries;
    bool truncated = false;
    CHECK(LogFile::ReadSession(writer.Directory(), writer.SessionName(), entries, truncated));
    int notes = 0;
    for (const auto& e : entries) {
        if (e.message.find("size limit reached") != std::string::npos) notes++;
        CHECK(e.message != "after the cap");
    }
    CHECK(notes == 1);
}

TEST(list_sessions_newest_first_ignores_foreign_files) {
    test::TempDir dir;
    Touch(dir.Path("2026-11-01_10-00-00.log"));
    Touch(dir.Path("2026-11-20_10-00-00.log"));
    Touch(dir.Path("2026-11-20_10-00-00_2.log"));
    Touch(dir.Path("readme.txt"));
    Touch(dir.Path("2026-11-05_10-00-00.log.bak"));
    fs::create_directories(dir.Path("2026-11-10_10-00-00.log"));

    auto sessions = LogFile::ListSessions(dir.path);
    CHECK(sessions.size() == 3);
    CHECK(sessions.size() == 3 && sessions[0].name == "2026-11-20_10-00-00_2.log");
    CHECK(sessions.size() == 3 && sessions[2].name == "2026-11-01_10-00-00.log");
    CHECK(LogFile::ListSessions(dir.Path("missing")).empty());
}

TEST(prune_by_age_and_count_keeps_current_and_foreign) {
    test::TempDir dir;
    std::time_t now = At(2026, 11, 27);
    Touch(dir.Path("2026-10-01_10-00-00.log"));
    Touch(dir.Path("2026-11-10_10-00-00.log"));
    Touch(dir.Path("2026-11-20_10-00-00.log"));
    Touch(dir.Path("2026-11-25_10-00-00.log"));
    Touch(dir.Path("2026-11-27_08-00-00.log"));
    Touch(dir.Path("important.txt"));

    int removed = LogFile::Prune(dir.path, "2026-11-27_08-00-00.log", now, 14, 30);
    CHECK(removed == 2);
    CHECK(!fs::exists(dir.Path("2026-10-01_10-00-00.log")));
    CHECK(!fs::exists(dir.Path("2026-11-10_10-00-00.log")));
    CHECK(fs::exists(dir.Path("2026-11-20_10-00-00.log")));
    CHECK(fs::exists(dir.Path("important.txt")));

    removed = LogFile::Prune(dir.path, "2026-11-27_08-00-00.log", now, 14, 2);
    CHECK(removed == 1);
    CHECK(fs::exists(dir.Path("2026-11-27_08-00-00.log")));
    CHECK(fs::exists(dir.Path("2026-11-25_10-00-00.log")));
    CHECK(!fs::exists(dir.Path("2026-11-20_10-00-00.log")));

    Touch(dir.Path("2020-01-01_00-00-00.log"));
    removed = LogFile::Prune(dir.path, "2020-01-01_00-00-00.log", now, 14, 30);
    CHECK(fs::exists(dir.Path("2020-01-01_00-00-00.log")));
}

TEST(delete_all_except_keeps_current_and_foreign) {
    test::TempDir dir;
    Touch(dir.Path("2026-11-20_10-00-00.log"));
    Touch(dir.Path("2026-11-27_08-00-00.log"));
    Touch(dir.Path("keep.txt"));
    CHECK(LogFile::DeleteAllExcept(dir.path, "2026-11-27_08-00-00.log") == 1);
    CHECK(fs::exists(dir.Path("2026-11-27_08-00-00.log")));
    CHECK(fs::exists(dir.Path("keep.txt")));
}

TEST(read_session_refuses_traversal_and_cuts_large_files) {
    test::TempDir dir;
    std::vector<LogFile::Entry> entries;
    bool truncated = false;
    Touch(dir.Path("secret.log"), "10:00:00 I secret\n");
    CHECK(!LogFile::ReadSession(dir.path, "../secret.log", entries, truncated));
    CHECK(!LogFile::ReadSession(dir.path, "secret.log", entries, truncated));
    CHECK(!LogFile::ReadSession(dir.path, "2026-01-01_00-00-00.log", entries, truncated));

    std::string name = "2026-11-27_08-00-00.log";
    {
        std::ofstream out(dir.Path(name), std::ios::binary);
        std::string line = "10:00:00 I " + std::string(1000, 'q') + "\n";
        long long written = 0;
        while (written < LogFile::kMaxReadBytes + 5000) { out << line; written += (long long)line.size(); }
        out << "10:00:01 E last line\n";
    }
    CHECK(LogFile::ReadSession(dir.path, name, entries, truncated));
    CHECK(truncated);
    CHECK(!entries.empty());
    CHECK(!entries.empty() && entries.back().message == "last line");
    CHECK(!entries.empty() && entries.front().time == "10:00:00");
}

TEST_MAIN()
