#pragma once
#include <string>
#include <vector>

struct CatalogEntry {
    std::string name;
    std::string repository;   // "owner/repo"
    std::string category;
    std::string description;
};

class ModuleImporter {
public:
    static std::string GetCatalogUrl();

    static bool FetchCatalog(std::vector<CatalogEntry>& outEntries, std::string& outError);

    static bool CloneFromRepository(const std::string& ownerRepo,
                                     const std::string& projectModulesDir,
                                     std::string& outError);

    static bool CopyFromDisk(const std::string& manifestPath,
                              const std::string& projectModulesDir,
                              std::string& outError);
};
