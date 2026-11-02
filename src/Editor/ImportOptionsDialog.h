#pragma once
#include "Core/ImportHandlerRegistry.h"
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

class Widget;
class PropertyPanel;

class ImportOptionsDialog {
public:
    static ImportOptionsDialog& Get();

    using ConfirmCallback = std::function<void(const std::map<std::string, std::string>& options)>;

    void Enqueue(const ImportHandler& handler, const std::string& sourcePath, ConfirmCallback onConfirm);
    bool IsOpen() const { return active; }
    void Render();

private:
    struct Request {
        ImportHandler handler;
        std::string sourcePath;
        ConfirmCallback onConfirm;
    };

    void Activate();
    void Finish(bool confirmed);
    bool IsVisible(const ImportOptionDef& def) const;
    std::vector<struct PropertyRow> BuildRows();
    void EnsureChrome();

    std::deque<Request> queue;
    bool active = false;
    Request current;
    std::map<std::string, std::string> values;
    std::map<std::string, std::map<std::string, std::string>> remembered;
    PropertyPanel* panel = nullptr;
    Widget* chrome = nullptr;
};
