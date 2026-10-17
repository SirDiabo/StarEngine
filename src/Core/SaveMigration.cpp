#include "SaveMigration.h"
#include <algorithm>
#include <mutex>

namespace {

const char* const kVersionKey = "@saveDataVersion";

std::vector<SaveMigration::MigrationStep>& Steps() {
    static std::vector<SaveMigration::MigrationStep> steps;
    return steps;
}

int& ProjectVersion() {
    static int version = 0;
    return version;
}

std::mutex& Mutex() {
    static std::mutex m;
    return m;
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

int FileVersion(const std::unordered_map<std::string, std::string>& entries) {
    auto it = entries.find(kVersionKey);
    if (it == entries.end()) return 0;
    int v = 0;
    try { v = std::stoi(it->second); } catch (...) { v = 0; }
    return v;
}

void InjectVersion(std::unordered_map<std::string, std::string>& entries) {
    entries[kVersionKey] = std::to_string(GetProjectVersion());
}

static std::string ActorPrefix(const std::string& key) {
    size_t dot = key.find('.');
    if (dot == std::string::npos) return key;
    return key.substr(0, dot);
}

static std::string RestAfterActor(const std::string& key) {
    size_t dot = key.find('.');
    if (dot == std::string::npos) return "";
    return key.substr(dot + 1);
}

static bool KeyStartsWithActor(const std::string& key, const std::string& actor) {
    if (key.size() <= actor.size()) return false;
    if (key.compare(0, actor.size(), actor) != 0) return false;
    return key[actor.size()] == '.';
}

int ApplyMigrations(const std::string& saveFileName, std::unordered_map<std::string, std::string>& entries) {
    (void)saveFileName;
    std::lock_guard<std::mutex> lock(Mutex());

    int current = FileVersion(entries);
    int target = GetProjectVersion();
    if (current >= target) return current;

    while (current < target) {
        bool progressed = false;
        for (const auto& step : Steps()) {
            if (step.fromVersion != current) continue;
            for (const auto& op : step.ops) {
                switch (op.type) {
                case MigrationOp::Type::RenameActor: {
                    std::string prefix = op.oldName + ".";
                    for (auto it = entries.begin(); it != entries.end();) {
                        if (KeyStartsWithActor(it->first, op.oldName)) {
                            std::string rest = RestAfterActor(it->first);
                            std::string newKey = op.newName + "." + rest;
                            entries[newKey] = it->second;
                            it = entries.erase(it);
                        } else {
                            ++it;
                        }
                    }
                    break;
                }
                case MigrationOp::Type::RenameVariable: {
                    std::string oldVarKey = op.actor + "." + op.oldName;
                    std::string newVarKey = op.actor + "." + op.newName;
                    std::string oldArrKey = op.actor + "." + op.oldName + "[]";
                    std::string newArrKey = op.actor + "." + op.newName + "[]";
                    auto it = entries.find(oldVarKey);
                    if (it != entries.end()) {
                        entries[newVarKey] = it->second;
                        entries.erase(it);
                    }
                    it = entries.find(oldArrKey);
                    if (it != entries.end()) {
                        entries[newArrKey] = it->second;
                        entries.erase(it);
                    }
                    break;
                }
                case MigrationOp::Type::RenameKey: {
                    std::string oldKey = op.actor + "." + op.oldName;
                    std::string newKey = op.actor + "." + op.newName;
                    auto it = entries.find(oldKey);
                    if (it != entries.end()) {
                        entries[newKey] = it->second;
                        entries.erase(it);
                    }
                    break;
                }
                case MigrationOp::Type::DeleteKey: {
                    entries.erase(op.actor + "." + op.oldName);
                    break;
                }
                }
            }
            current = step.toVersion;
            progressed = true;
            break;
        }
        if (!progressed) break;
    }

    InjectVersion(entries);
    return current;
}

} // namespace SaveMigration
