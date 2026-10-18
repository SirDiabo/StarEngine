#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace SaveMigration {

struct MigrationOp {
    enum class Type { RenameActor, RenameVariable, RenameKey, DeleteKey };
    Type type = Type::RenameKey;
    std::string actor;
    std::string oldName;
    std::string newName;
};

struct MigrationStep {
    int fromVersion = 0;
    int toVersion = 1;
    std::vector<MigrationOp> ops;
};

using EntryMap = std::unordered_map<std::string, std::string>;

void Register(int fromVersion, int toVersion, std::vector<MigrationOp> ops);
void SetProjectVersion(int version);
int GetProjectVersion();

void Load(const std::string& projectRoot);
void Save();
std::vector<MigrationStep> GetSteps();
void SetSteps(std::vector<MigrationStep> steps);

bool Migrate(const std::string& saveFileName, EntryMap& entries);
int ApplyMigrations(const std::string& saveFileName, EntryMap& entries);
int FileVersion(const EntryMap& entries);
void InjectVersion(EntryMap& entries);

} // namespace SaveMigration
