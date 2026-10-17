#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace SaveMigration {

struct MigrationOp {
    enum class Type { RenameActor, RenameVariable, RenameKey, DeleteKey };
    Type type;
    std::string actor;
    std::string oldName;
    std::string newName;
};

struct MigrationStep {
    int fromVersion;
    int toVersion;
    std::vector<MigrationOp> ops;
};

void Register(int fromVersion, int toVersion, std::vector<MigrationOp> ops);
void SetProjectVersion(int version);
int GetProjectVersion();

int ApplyMigrations(const std::string& saveFileName, std::unordered_map<std::string, std::string>& entries);
int FileVersion(const std::unordered_map<std::string, std::string>& entries);
void InjectVersion(std::unordered_map<std::string, std::string>& entries);

} // namespace SaveMigration
