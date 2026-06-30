#pragma once
#include <string>

// Opens a native OS file dialog. Returns the selected path, or empty string if cancelled.
// defaultDir: initial directory shown (empty = current working directory).
// filter: description and extension pairs, e.g. "Layout Files\0*.cfg\0All Files\0*.*\0"
//         On non-Windows platforms this is ignored and all files are shown.
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
};
