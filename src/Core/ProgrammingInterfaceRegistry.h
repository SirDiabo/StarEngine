#pragma once
#include "Core/SectionFileIO.h"
#include <ostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <utility>

class Panel;
struct DockNode;
class WidgetBehavior;
class LogPanel;
class ViewportPanel;

class IBehaviorContent {
public:
    virtual ~IBehaviorContent() = default;
};

struct ProgrammingInterfaceContext {
    std::string assetKind;
    IBehaviorContent* content = nullptr;
    LogPanel* sharedLogPanel = nullptr;
    ViewportPanel* sharedViewportPanel = nullptr;
    std::function<std::vector<std::pair<std::string, std::string>>()> listElements;
    std::function<void()> onContentDirtied;
    std::function<void()> onSaveRequested;
};

struct ProgrammingInterfacePanelSet {
    std::vector<Panel*> panels;
    std::unique_ptr<DockNode> layout;
    std::function<void()> syncBeforeSave;
    std::function<void()> teardown;
    std::function<void(const std::string& elementName, const std::string& elementClass)> onElementDragStarted;
};

struct ProgrammingInterfaceInfo {
    std::string name;
    std::string displayLabel;
    std::function<IBehaviorContent*()> createDefaultContent;
    std::function<void(std::ostream&, const IBehaviorContent&, const std::string& prefix)> saveContent;
    std::function<IBehaviorContent*(const std::vector<SectionFileIO::ParsedSection>&, const std::string& prefix)> loadContent;
    std::function<WidgetBehavior*(IBehaviorContent&)> createWidgetBehavior;
    std::function<ProgrammingInterfacePanelSet(const ProgrammingInterfaceContext&)> buildWidgetEditorPanels;
};

class ProgrammingInterfaceRegistry {
public:
    static ProgrammingInterfaceRegistry& Get();

    void RegisterInterface(const ProgrammingInterfaceInfo& info);
    const ProgrammingInterfaceInfo* Find(const std::string& name) const;
    const std::vector<ProgrammingInterfaceInfo>& GetAll() const { return interfaces; }

private:
    std::vector<ProgrammingInterfaceInfo> interfaces;
};
