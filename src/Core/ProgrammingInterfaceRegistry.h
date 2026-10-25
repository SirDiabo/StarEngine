#pragma once
#include "Runtime/ComponentDecl.h"
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
class ActorBehavior;
class EditorTab;
struct EditorServices;
class LogPanel;
class ViewportPanel;
class DetailsPanel;

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
    std::function<void()> revealOwnerTab;
    std::function<bool()> isOwnerTabActive;
};

struct ProgrammingInterfacePanelSet {
    std::vector<Panel*> panels;
    DetailsPanel* detailsPanel = nullptr;
    std::function<std::string()> detailsSelectionKey;
    std::string listPanelName;
    std::string mainPanelName;
    std::unique_ptr<DockNode> layout;
    std::function<void()> syncBeforeSave;
    std::function<void()> teardown;
    std::function<void(const std::string& elementName, const std::string& elementClass)> onElementDragStarted;
};

struct ProgrammingInterfaceInfo {
    std::string name;
    std::string displayLabel;
    std::string moduleName;
    std::string legacyActorSection;
    std::function<IBehaviorContent*()> createDefaultContent;
    std::function<void(std::ostream&, const IBehaviorContent&, const std::string& prefix)> saveContent;
    std::function<IBehaviorContent*(const std::vector<SectionFileIO::ParsedSection>&, const std::string& prefix)> loadContent;
    std::function<WidgetBehavior*(IBehaviorContent&)> createWidgetBehavior;
    std::function<ProgrammingInterfacePanelSet(const ProgrammingInterfaceContext&)> buildWidgetEditorPanels;

    std::function<IBehaviorContent*()> createActorContent;
    std::function<void(std::ostream&, const IBehaviorContent&, const std::string& prefix)> saveActorContent;
    std::function<IBehaviorContent*(const std::vector<SectionFileIO::ParsedSection>&, const std::string& prefix)> loadActorContent;
    std::function<ActorBehavior*(IBehaviorContent&, const std::vector<ComponentDecl>& components)> createActorBehavior;
    std::function<EditorTab*(const std::string& fullPath, const EditorServices& services)> createActorEditorTab;

    bool SupportsWidgets() const { return static_cast<bool>(createWidgetBehavior); }
    bool SupportsActors() const { return static_cast<bool>(createActorBehavior); }
    bool IsAvailable() const;
};

class ProgrammingInterfaceRegistry {
public:
    static ProgrammingInterfaceRegistry& Get();

    void RegisterInterface(const ProgrammingInterfaceInfo& info);
    const ProgrammingInterfaceInfo* Find(const std::string& name) const;
    const std::vector<ProgrammingInterfaceInfo>& GetAll() const { return interfaces; }

    std::vector<const ProgrammingInterfaceInfo*> GetAvailableForWidgets() const;
    std::vector<const ProgrammingInterfaceInfo*> GetAvailableForActors() const;
    const ProgrammingInterfaceInfo* FindByLegacyActorSection(const std::string& sectionName) const;

private:
    std::vector<ProgrammingInterfaceInfo> interfaces;
};
