#include "SaveMigration.h"
#include "LayoutSerializer.h"
#include "SafeParse.h"
#include "raylib.h"
#include <filesystem>
#include <mutex>

namespace fs = std::filesystem;

namespace {

using SaveMigration::EntryMap;
using SaveMigration::MigrationOp;
using SaveMigration::MigrationStep;

const char* const kVersionKey = "@saveDataVersion";

std::mutex& Mutex() {
    static std::mutex m;
    return m;
}

std::vector<MigrationStep>& Steps() {
    static std::vector<MigrationStep> steps;
    return steps;
}

int& ProjectVersion() {
    static int version = 1;
    return version;
}

std::string& ConfigPath() {
    static std::string path;
    return path;
}

int FileVersionLocked(const EntryMap& entries) {
    auto it = entries.find(kVersionKey);
    if (it == entries.end()) return 0;
    return SafeParse::ToInt(it->second);
}

bool KeyStartsWithActor(const std::string& key, const std::string& actor) {
    return key.size() > actor.size() && key.compare(0, actor.size(), actor) == 0 && key[actor.size()] == '.';
}

void MoveKey(EntryMap& entries, const std::string& from, const std::string& to) {
    if (from == to) return;
    auto it = entries.find(from);
    if (it == entries.end()) return;
    std::string value = std::move(it->second);
    entries.erase(it);
    entries[to] = std::move(value);
}

void ApplyOp(EntryMap& entries, const MigrationOp& op) {
    switch (op.type) {
    case MigrationOp::Type::RenameActor: {
        std::vector<std::string> matching;
        for (const auto& kv : entries) {
            if (KeyStartsWithActor(kv.first, op.oldName)) matching.push_back(kv.first);
        }
        for (const std::string& key : matching) {
            MoveKey(entries, key, op.newName + key.substr(op.oldName.size()));
        }
        break;
    }
    case MigrationOp::Type::RenameVariable:
        MoveKey(entries, op.actor + "." + op.oldName, op.actor + "." + op.newName);
        MoveKey(entries, op.actor + "." + op.oldName + "[]", op.actor + "." + op.newName + "[]");
        break;
    case MigrationOp::Type::RenameKey:
        MoveKey(entries, op.actor + "." + op.oldName, op.actor + "." + op.newName);
        break;
    case MigrationOp::Type::DeleteKey:
        entries.erase(op.actor + "." + op.oldName);
        entries.erase(op.actor + "." + op.oldName + "[]");
        break;
    }
}

std::string EscapeField(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '|') out += "\\p";
        else out += c;
    }
    return out;
}

std::vector<std::string> SplitFields(const std::string& s) {
    std::vector<std::string> fields(1);
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '|') { fields.emplace_back(); continue; }
        if (c == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            fields.back() += (n == 'p') ? '|' : n;
            continue;
        }
        fields.back() += c;
    }
    return fields;
}

} // namespace

namespace SaveMigration {

void Register(int fromVersion, int toVersion, std::vector<MigrationOp> ops) {
    std::lock_guard<std::mutex> lock(Mutex());
    MigrationStep step;
    step.fromVersion = fromVersion;
    step.toVersion = toVersion;
    step.ops = std::move(ops);
    Steps().push_back(std::move(step));
}

void SetProjectVersion(int version) {
    std::lock_guard<std::mutex> lock(Mutex());
    ProjectVersion() = version;
}

int GetProjectVersion() {
    std::lock_guard<std::mutex> lock(Mutex());
    return ProjectVersion();
}

std::vector<MigrationStep> GetSteps() {
    std::lock_guard<std::mutex> lock(Mutex());
    return Steps();
}

void SetSteps(std::vector<MigrationStep> steps) {
    std::lock_guard<std::mutex> lock(Mutex());
    Steps() = std::move(steps);
}

void Load(const std::string& projectRoot) {
    std::lock_guard<std::mutex> lock(Mutex());
    Steps().clear();
    ProjectVersion() = 1;
    ConfigPath() = (fs::path(projectRoot) / "Config" / "SaveMigrations.cfg").string();
    std::unordered_map<std::string, std::string> raw;
    if (!LayoutSerializer::Load(ConfigPath(), raw)) return;
    auto get = [&raw](const std::string& key) -> const std::string* {
        auto it = raw.find(key);
        return it == raw.end() ? nullptr : &it->second;
    };
    if (const std::string* v = get("version")) ProjectVersion() = SafeParse::ToInt(*v, 1);
    int stepCount = 0;
    if (const std::string* v = get("steps")) stepCount = SafeParse::ToInt(*v);
    for (int i = 0; i < stepCount; i++) {
        std::string prefix = "step." + std::to_string(i);
        const std::string* range = get(prefix);
        if (!range) continue;
        std::vector<std::string> bounds = SplitFields(*range);
        if (bounds.size() < 2) continue;
        MigrationStep step;
        step.fromVersion = SafeParse::ToInt(bounds[0]);
        step.toVersion = SafeParse::ToInt(bounds[1]);
        for (int j = 0;; j++) {
            const std::string* opText = get(prefix + ".op." + std::to_string(j));
            if (!opText) break;
            std::vector<std::string> f = SplitFields(*opText);
            if (f.size() < 4) continue;
            MigrationOp op;
            op.type = (MigrationOp::Type)SafeParse::ToInt(f[0]);
            op.actor = f[1];
            op.oldName = f[2];
            op.newName = f[3];
            step.ops.push_back(std::move(op));
        }
        Steps().push_back(std::move(step));
    }
}

void Save() {
    std::lock_guard<std::mutex> lock(Mutex());
    if (ConfigPath().empty()) return;
    std::unordered_map<std::string, std::string> raw;
    raw["version"] = std::to_string(ProjectVersion());
    raw["steps"] = std::to_string(Steps().size());
    for (size_t i = 0; i < Steps().size(); i++) {
        const MigrationStep& step = Steps()[i];
        std::string prefix = "step." + std::to_string(i);
        raw[prefix] = std::to_string(step.fromVersion) + "|" + std::to_string(step.toVersion);
        for (size_t j = 0; j < step.ops.size(); j++) {
            const MigrationOp& op = step.ops[j];
            raw[prefix + ".op." + std::to_string(j)] = std::to_string((int)op.type) + "|" + EscapeField(op.actor) +
                                                       "|" + EscapeField(op.oldName) + "|" + EscapeField(op.newName);
        }
    }
    LayoutSerializer::Save(ConfigPath(), raw);
}

int FileVersion(const EntryMap& entries) {
    return FileVersionLocked(entries);
}

void InjectVersion(EntryMap& entries) {
    std::lock_guard<std::mutex> lock(Mutex());
    entries[kVersionKey] = std::to_string(ProjectVersion());
}

bool Migrate(const std::string& saveFileName, EntryMap& entries) {
    std::lock_guard<std::mutex> lock(Mutex());
    int current = FileVersionLocked(entries);
    const int target = ProjectVersion();
    if (current >= target) return false;

    while (current < target) {
        const MigrationStep* next = nullptr;
        for (const MigrationStep& step : Steps()) {
            if (step.fromVersion == current && step.toVersion > current) { next = &step; break; }
        }
        if (!next) {
            TraceLog(LOG_WARNING, "SaveMigration: '%s' has no migration step from version %d; treating versions up to %d as unchanged",
                     saveFileName.c_str(), current, target);
            break;
        }
        for (const MigrationOp& op : next->ops) ApplyOp(entries, op);
        current = next->toVersion;
    }
    entries[kVersionKey] = std::to_string(target);
    return true;
}

int ApplyMigrations(const std::string& saveFileName, EntryMap& entries) {
    Migrate(saveFileName, entries);
    return FileVersion(entries);
}

} // namespace SaveMigration
