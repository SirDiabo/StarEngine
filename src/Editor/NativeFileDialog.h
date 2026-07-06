#pragma once
#include <string>

class NativeFileDialog {
public:
    static std::string OpenFile(const std::string& title,
                                const std::string& defaultDir,
                                const std::string& filterDesc,
                                const std::string& filterExt);

    static std::string SaveFile(const std::string& title,
                                const std::string& defaultDir,
                                const std::string& defaultName,
                                const std::string& filterDesc,
                                const std::string& filterExt);

    static std::string OpenFolder(const std::string& title, const std::string& defaultDir);
};
