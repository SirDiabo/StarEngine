#pragma once
#include <string>
#include <vector>

class TrustStore {
public:
    explicit TrustStore(std::string directory);

    static std::string KeyFor(const std::string& projectRoot);

    bool IsTrusted(const std::string& projectRoot) const;
    bool Trust(const std::string& projectRoot);
    bool Revoke(const std::string& projectRoot);
    std::vector<std::string> TrustedKeys() const;

    std::string MachineToken();

    const std::string& Directory() const { return directory; }

private:
    std::string TrustFilePath() const;
    std::string TokenFilePath() const;
    std::vector<std::string> LoadKeys() const;
    bool SaveKeys(const std::vector<std::string>& keys) const;

    std::string directory;
    std::string cachedToken;
};
