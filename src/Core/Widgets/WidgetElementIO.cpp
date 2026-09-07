#include "WidgetElementIO.h"
#include "WidgetElement.h"
#include "WidgetElementFields.h"
#include "TabContainer.h"
#include "Core/SafeParse.h"
#include <unordered_map>

namespace {

using SectionFileIO::ParsedSection;
using SectionFileIO::FindEntry;

std::unordered_map<std::string, WidgetElement*> defaultInstanceCache;

WidgetElement* GetDefaultInstance(const std::string& elementType) {
    auto it = defaultInstanceCache.find(elementType);
    if (it != defaultInstanceCache.end()) return it->second;
    WidgetElement* instance = CreateDefaultWidgetElement(elementType);
    defaultInstanceCache[elementType] = instance;
    return instance;
}

void ClearDefaultInstanceCache() {
    for (auto& kv : defaultInstanceCache) delete kv.second;
    defaultInstanceCache.clear();
}

void WriteElement(std::ostream& out, WidgetElement* element, const std::string& parentId) {
    out << "[Element" << element->elementId << "]\n";
    out << "Type=" << element->GetClassName() << "\n";
    if (!parentId.empty()) out << "Parent=" << parentId << "\n";
    if (element->anchor.x != 0.0f || element->anchor.y != 0.0f)
        out << "Anchor=" << element->anchor.x << "," << element->anchor.y << "\n";
    if (element->pivot.x != 0.0f || element->pivot.y != 0.0f)
        out << "Pivot=" << element->pivot.x << "," << element->pivot.y << "\n";
    if (element->offset.x != 0.0f || element->offset.y != 0.0f)
        out << "Offset=" << element->offset.x << "," << element->offset.y << "\n";
    if (element->size.x != 100.0f || element->size.y != 30.0f)
        out << "Size=" << element->size.x << "," << element->size.y << "\n";
    if (!element->visible) out << "Visible=false\n";
    if (element->selectable) out << "Selectable=true\n";
    if (element->disabled) out << "Disabled=true\n";

    std::string elementType = element->GetClassName();
    WidgetElement* defaultInstance = GetDefaultInstance(elementType);
    for (const WidgetElementFieldDesc& field : GetWidgetElementFields()) {
        if (field.elementType != elementType) continue;
        std::string current = field.getValue(*element);
        std::string defaultValue = defaultInstance ? field.getValue(*defaultInstance) : "";
        if (current != defaultValue) out << field.key << "=" << current << "\n";
    }
    out << "\n";

    for (WidgetElement* child : element->GetChildren()) {
        WriteElement(out, child, element->elementId);
    }
}

void RelinkTabHeaderButtons(WidgetElement* element) {
    if (!element) return;
    if (TabContainer* tabContainer = dynamic_cast<TabContainer*>(element)) {
        const auto& kids = tabContainer->GetChildren();
        if (!kids.empty()) {
            WidgetElement* headerBox = kids[0];
            const auto& buttons = headerBox->GetChildren();
            for (size_t i = 0; i < buttons.size(); i++) {
                if (TabHeaderButtonElement* button = dynamic_cast<TabHeaderButtonElement*>(buttons[i])) {
                    button->owningTabContainer = tabContainer;
                    button->tabIndex = (int)i;
                }
            }
        }
    }
    for (WidgetElement* child : element->GetChildren()) {
        RelinkTabHeaderButtons(child);
    }
}

}

namespace WidgetElementIO {

void WriteElementTree(std::ostream& out, WidgetElement* root) {
    int elementCount = 0;
    std::vector<WidgetElement*> stack;
    if (root) stack.push_back(root);
    while (!stack.empty()) {
        WidgetElement* current = stack.back();
        stack.pop_back();
        elementCount++;
        for (WidgetElement* child : current->GetChildren()) stack.push_back(child);
    }

    out << "[Elements]\n";
    out << "Count=" << elementCount << "\n";
    out << "\n";

    if (root) WriteElement(out, root, "");

    ClearDefaultInstanceCache();
}

WidgetElement* ReadElementTree(const std::vector<ParsedSection>& sections) {
    WidgetElement* rootElement = nullptr;

    struct PendingElement {
        WidgetElement* element = nullptr;
        std::string parentId;
    };
    std::unordered_map<std::string, PendingElement> pendingById;
    std::vector<std::string> orderedIds;

    for (const ParsedSection& section : sections) {
        if (section.name == "Elements" || section.name.rfind("Element", 0) != 0) continue;

        const std::string* typeValue = FindEntry(section, "Type");
        if (!typeValue) continue;

        WidgetElement* element = CreateDefaultWidgetElement(*typeValue);
        if (!element) continue;
        element->ClearChildren();

        std::string elementId = section.name.substr(std::string("Element").size());
        element->elementId = elementId;
        WidgetElement::EnsureNextElementIdAtLeast(SafeParse::ToInt(elementId));

        if (const std::string* v = FindEntry(section, "Anchor")) {
            size_t comma = v->find(',');
            if (comma != std::string::npos) {
                element->anchor.x = SafeParse::ToFloat(v->substr(0, comma));
                element->anchor.y = SafeParse::ToFloat(v->substr(comma + 1));
            }
        }
        if (const std::string* v = FindEntry(section, "Pivot")) {
            size_t comma = v->find(',');
            if (comma != std::string::npos) {
                element->pivot.x = SafeParse::ToFloat(v->substr(0, comma));
                element->pivot.y = SafeParse::ToFloat(v->substr(comma + 1));
            }
        }
        if (const std::string* v = FindEntry(section, "Offset")) {
            size_t comma = v->find(',');
            if (comma != std::string::npos) {
                element->offset.x = SafeParse::ToFloat(v->substr(0, comma));
                element->offset.y = SafeParse::ToFloat(v->substr(comma + 1));
            }
        }
        if (const std::string* v = FindEntry(section, "Size")) {
            size_t comma = v->find(',');
            if (comma != std::string::npos) {
                element->size.x = SafeParse::ToFloat(v->substr(0, comma));
                element->size.y = SafeParse::ToFloat(v->substr(comma + 1));
            }
        }
        if (const std::string* v = FindEntry(section, "Visible")) element->visible = (*v == "true");
        if (const std::string* v = FindEntry(section, "Selectable")) element->selectable = (*v == "true");
        if (const std::string* v = FindEntry(section, "Disabled")) element->disabled = (*v == "true");

        for (const WidgetElementFieldDesc& field : GetWidgetElementFields()) {
            if (field.elementType != *typeValue) continue;
            if (const std::string* v = FindEntry(section, field.key)) field.setValue(*element, *v);
        }

        PendingElement pending;
        pending.element = element;
        if (const std::string* v = FindEntry(section, "Parent")) pending.parentId = *v;
        pendingById[elementId] = pending;
        orderedIds.push_back(elementId);
    }

    for (const std::string& id : orderedIds) {
        PendingElement& pending = pendingById[id];
        if (pending.parentId.empty()) {
            if (!rootElement) rootElement = pending.element;
            continue;
        }
        auto parentIt = pendingById.find(pending.parentId);
        if (parentIt != pendingById.end()) {
            parentIt->second.element->AddChild(pending.element);
        } else if (!rootElement) {
            rootElement = pending.element;
        }
    }

    RelinkTabHeaderButtons(rootElement);

    return rootElement;
}

}
