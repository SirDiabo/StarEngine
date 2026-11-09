#pragma once
#include "Core/AudioEffectRegistry.h"
#include "Editor/RegistryEditorRows.h"
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

inline void AddEffectListRows(std::vector<PropertyRow>& rows, const std::string& id, const std::vector<std::string>& items,
                              std::function<void(const std::vector<std::string>&)> apply) {
    const std::vector<std::string> available = AudioEffectRegistry::Get().Names();
    rows.push_back(PropertyRows::HeaderRow(id + ".h", "Effects"));
    for (size_t i = 0; i < items.size(); i++) {
        std::vector<std::string> options = available;
        if (std::find(available.begin(), available.end(), items[i]) == available.end()) options.push_back(items[i]);
        const std::string rowId = id + "." + std::to_string(i);
        const bool known = AudioEffectRegistry::Get().Find(items[i]) != nullptr;
        const bool expanded = ExpandedEffectRows().count(rowId) > 0;
        if (known) options.push_back(expanded ? "-- Hide Settings" : "-- Edit Settings");
        if (i > 0) options.push_back("-- Move Up");
        options.push_back("-- Remove");
        PropertyRow row = PropertyRows::EnumRow(rowId, std::to_string(i + 1), items[i], options,
            [apply, items, i, rowId](const std::string& v) {
                if (v == "-- Edit Settings") { ExpandedEffectRows().insert(rowId); return; }
                if (v == "-- Hide Settings") { ExpandedEffectRows().erase(rowId); return; }
                std::vector<std::string> next = items;
                if (v == "-- Remove") next.erase(next.begin() + (long)i);
                else if (v == "-- Move Up") std::swap(next[i], next[i - 1]);
                else next[i] = v;
                apply(next);
            });
        row.dropdown = true;
        rows.push_back(row);
        if (expanded && known) AddEffectSettingsRows(rows, rowId + ".fx", items[i]);
    }
    PropertyRow add = PropertyRows::EnumRow(id + ".add", "Add", available.empty() ? "No effects defined" : "Add effect...", available,
        [apply, items](const std::string& v) {
            std::vector<std::string> next = items;
            next.push_back(v);
            apply(next);
        });
    add.dropdown = true;
    rows.push_back(add);
}
