#pragma once
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace test {

struct Case {
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& Registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { Registry().push_back({ name, fn }); }
};

struct State {
    int failedChecks = 0;
    int totalChecks = 0;
    bool caseFailed = false;
    const char* currentCase = "";
};

inline State& Current() {
    static State state;
    return state;
}

inline void ReportFailure(const char* file, int line, const std::string& message) {
    State& s = Current();
    s.failedChecks++;
    s.caseFailed = true;
    std::printf("    FAILED %s:%d\n      %s\n", file, line, message.c_str());
    std::fflush(stdout);
}

inline std::string Hex(const uint8_t* data, size_t size) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (size_t i = 0; i < size; i++) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 15];
    }
    return out;
}

inline std::vector<uint8_t> FromHex(const std::string& text) {
    std::vector<uint8_t> out;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    int high = -1;
    for (char c : text) {
        int n = nibble(c);
        if (n < 0) continue;
        if (high < 0) {
            high = n;
        } else {
            out.push_back((uint8_t)((high << 4) | n));
            high = -1;
        }
    }
    return out;
}

inline bool BytesEqual(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size()) == 0);
}

inline std::string DescribeMismatch(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected) {
    if (actual.size() != expected.size()) {
        return "size " + std::to_string(actual.size()) + " != expected " + std::to_string(expected.size());
    }
    for (size_t i = 0; i < actual.size(); i++) {
        if (actual[i] != expected[i]) {
            return "first difference at byte " + std::to_string(i) + ": got " + Hex(&actual[i], 1) + ", expected " + Hex(&expected[i], 1);
        }
    }
    return "equal";
}

class Rng {
public:
    explicit Rng(uint64_t seed) : state(seed ? seed : 0x9E3779B97F4A7C15ull) {}

    uint64_t Next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }

    uint32_t Below(uint32_t bound) { return bound ? (uint32_t)(Next() % bound) : 0u; }

    std::vector<uint8_t> Bytes(size_t count) {
        std::vector<uint8_t> out(count);
        for (uint8_t& b : out) b = (uint8_t)Next();
        return out;
    }

private:
    uint64_t state;
};

inline int RunAll(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    State& s = Current();
    int ran = 0;
    int failedCases = 0;
    std::vector<std::string> failedNames;
    auto suiteStart = std::chrono::steady_clock::now();

    for (const Case& c : Registry()) {
        if (filter && std::strstr(c.name, filter) == nullptr) continue;
        s.currentCase = c.name;
        s.caseFailed = false;
        std::printf("[ RUN    ] %s\n", c.name);
        std::fflush(stdout);
        auto start = std::chrono::steady_clock::now();
        c.fn();
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        ran++;
        if (s.caseFailed) {
            failedCases++;
            failedNames.push_back(c.name);
            std::printf("[ FAILED ] %s (%lld ms)\n", c.name, ms);
        } else {
            std::printf("[     OK ] %s (%lld ms)\n", c.name, ms);
        }
        std::fflush(stdout);
    }

    long long totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - suiteStart).count();
    std::printf("\n%d test(s) run, %d failed, %d checks, %d failed checks (%lld ms)\n", ran, failedCases, s.totalChecks, s.failedChecks, totalMs);
    for (const std::string& name : failedNames) std::printf("  failed: %s\n", name.c_str());
    if (ran == 0) {
        std::printf("no tests matched\n");
        return 1;
    }
    return failedCases == 0 ? 0 : 1;
}

}

#define TEST(name)                                           \
    static void name();                                      \
    static ::test::Registrar registrar_##name(#name, name);  \
    static void name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        ::test::Current().totalChecks++;                                         \
        if (!(cond)) ::test::ReportFailure(__FILE__, __LINE__, "CHECK(" #cond ")"); \
    } while (0)

#define CHECK_MSG(cond, message)                                                 \
    do {                                                                         \
        ::test::Current().totalChecks++;                                         \
        if (!(cond)) ::test::ReportFailure(__FILE__, __LINE__, std::string("CHECK(" #cond ") ") + (message)); \
    } while (0)

#define REQUIRE(cond)                                                            \
    do {                                                                         \
        ::test::Current().totalChecks++;                                         \
        if (!(cond)) {                                                           \
            ::test::ReportFailure(__FILE__, __LINE__, "REQUIRE(" #cond ")");     \
            return;                                                              \
        }                                                                        \
    } while (0)

#define CHECK_EQ_STR(actual, expected)                                           \
    do {                                                                         \
        ::test::Current().totalChecks++;                                         \
        std::string actualValue_ = (actual);                                     \
        std::string expectedValue_ = (expected);                                 \
        if (actualValue_ != expectedValue_) {                                    \
            ::test::ReportFailure(__FILE__, __LINE__, "got \"" + actualValue_ + "\" expected \"" + expectedValue_ + "\" from " #actual); \
        }                                                                        \
    } while (0)

#define CHECK_BYTES(actual, expected)                                            \
    do {                                                                         \
        ::test::Current().totalChecks++;                                         \
        if (!::test::BytesEqual((actual), (expected))) {                         \
            ::test::ReportFailure(__FILE__, __LINE__, ::test::DescribeMismatch((actual), (expected)) + " in " #actual); \
        }                                                                        \
    } while (0)

#define TEST_MAIN()                                                              \
    int main(int argc, char** argv) { return ::test::RunAll(argc, argv); }
