#include "TreeListElement.h"
#include "TextElement.h"
#include "ButtonElement.h"
#include "RectangleElement.h"
#include "DragDropController.h"
#include "Widget.h"
#include "WidgetInput.h"
#include "raylib.h"
#include <cstdint>
#include <string>

namespace {

bool EntriesEqual(const std::vector<TreeListElement::Entry>& a, const std::vector<TreeListElement::Entry>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        const TreeListElement::Entry& ea = a[i];
        const TreeListElement::Entry& eb = b[i];
        if (ea.id != eb.id || ea.label != eb.label || ea.parentId != eb.parentId || ea.isCurrent != eb.isCurrent ||
            ea.iconColor.r != eb.iconColor.r || ea.iconColor.g != eb.iconColor.g ||
            ea.iconColor.b != eb.iconColor.b || ea.iconColor.a != eb.iconColor.a) {
            return false;
        }
    }
    return true;
}

}

TreeListElement::TreeListElement() {
    axis = BoxAxis::Vertical;
    scrollEnabled = true;
}

void TreeListElement::SetEntries(std::vector<Entry> newEntries) {
    bool changed = !EntriesEqual(entries, newEntries);
    if (changed) entries = std::move(newEntries);
    if (changed || needsDeferredRebuild) {
        needsDeferredRebuild = false;
        RebuildRows();
    }
}

void TreeListElement::BeginRename(const std::string& id) {
    if (!allowRename) return;
    renamingId = id;
    RebuildRows();
}

void TreeListElement::AddRowRecursive(int entryIndex, int depth, int& visualIndex) {
    const Entry& entry = entries[entryIndex];

    RowInfo info;
    info.id = entry.id;
    rowInfos.push_back(info);

    auto* row = new ButtonElement();
    row->size = { 0.0f, rowHeight };
    Color baseColor = (visualIndex % 2 == 1) ? rowAltColor : Color{ 0, 0, 0, 0 };
    row->defaultColor = entry.isCurrent ? rowCurrentColor : baseColor;
    row->hoveredColor = rowHoveredColor;
    row->selectedColor = rowHoveredColor;
    row->pressedColor = rowHoveredColor;

    std::string id = entry.id;
    std::string label = entry.label;
    bool draggable = allowReorder || allowReparent || allowExternalDrag;
    std::string kind = dragKind;
    bool renameEnabled = allowRename;
    bool isRenamingThisRow = allowRename && entry.id == renamingId;
    if (!isRenamingThisRow) {
        row->onActivated = [this, id, label, draggable, kind, renameEnabled]() {
            if (onEntrySelected) onEntrySelected(id);

            bool startedRename = false;
            if (renameEnabled) {
                float now = (float)GetTime();
                if (lastClickedId == id && (now - lastClickTime) < 0.35f) {
                    renamingId = id;
                    startedRename = true;
                    RebuildRows();
                }
                lastClickedId = id;
                lastClickTime = now;
            }

            if (draggable && !startedRename) DragDrop::Begin(kind, id, label, GetMousePosition());
        };
        row->onRightActivated = [this, id]() {
            if (onEntryRightClicked) onEntryRightClicked(id);
        };
    }

    float textOffset = 8.0f + depth * indentWidth;
    if (entry.iconColor.a > 0) {
        auto* swatch = new RectangleElement();
        swatch->color = entry.iconColor;
        swatch->anchor = { 0.0f, 0.5f };
        swatch->pivot = { 0.0f, 0.5f };
        swatch->offset = { textOffset, -4.0f };
        swatch->size = { 8.0f, 8.0f };
        row->AddChild(swatch);
        textOffset += 14.0f;
    }

    auto* label_ = new TextElement();
    label_->text = entry.label;
    label_->color = entry.isCurrent ? rowCurrentTextColor : rowColor;
    label_->fontSize = 11.0f;
    label_->anchor = { 0.0f, 0.5f };
    label_->pivot = { 0.0f, 0.5f };
    label_->offset = { textOffset, 0.0f };
    label_->size = { 0.0f, 16.0f };

    if (isRenamingThisRow) {
        float available = lastResolvedRect.width - textOffset - 12.0f;
        label_->size.x = available > 80.0f ? available : 160.0f;
        label_->editable = true;
        label_->onTextCommitted = [this, id](const std::string& newName) {
            if (onRenameRequested && !newName.empty()) onRenameRequested(id, newName);
            renamingId.clear();
            needsDeferredRebuild = true;
        };
        label_->onEditCancelled = [this]() {
            renamingId.clear();
            needsDeferredRebuild = true;
        };
    }

    row->AddChild(label_);

    if (isRenamingThisRow) label_->StartEditing();

    row->selectionKey = std::to_string(reinterpret_cast<uintptr_t>(this)) + ":" + entry.id;
    AddChild(row);
    visualIndex++;

    for (int i = 0; i < (int)entries.size(); i++) {
        if (entries[i].parentId == entry.id) AddRowRecursive(i, depth + 1, visualIndex);
    }
}

void TreeListElement::RebuildRows() {
    ClearChildren();
    rowInfos.clear();

    int visualIndex = 0;
    for (int i = 0; i < (int)entries.size(); i++) {
        if (entries[i].parentId.empty()) AddRowRecursive(i, 0, visualIndex);
    }
}

void TreeListElement::OnTickSelf(float deltaTime, Rectangle rect, float uiScale) {
    if (!onDeleteRequested || !owningWidget || !owningWidget->currentSelection) return;
    if (!IsKeyPressed(KEY_DELETE)) return;
    if (!WidgetInput::WidgetHasKeyboard(owningWidget)) return;

    const std::vector<WidgetElement*>& rowElements = GetChildren();
    for (size_t i = 0; i < rowElements.size() && i < rowInfos.size(); i++) {
        if (rowElements[i] == owningWidget->currentSelection) {
            onDeleteRequested(rowInfos[i].id);
            return;
        }
    }
}

void TreeListElement::Draw(Rectangle rect, float uiScale) {
    BoxElement::Draw(rect, uiScale);

    if (!DragDrop::Matches(dragKind)) {
        externalDragFired = false;
        return;
    }

    if (allowExternalDrag && !externalDragFired) {
        externalDragFired = true;
        if (onExternalDragStarted) onExternalDragStarted(DragDrop::Payload());
    }

    Vector2 mouse = GetMousePosition();
    if (!CheckCollisionPointRec(mouse, rect)) return;

    const std::vector<WidgetElement*>& rowElements = GetChildren();
    for (size_t i = 0; i < rowElements.size() && i < rowInfos.size(); i++) {
        Rectangle rowRect = rowElements[i]->lastResolvedRect;
        if (!CheckCollisionPointRec(mouse, rowRect)) continue;
        if (rowInfos[i].id == DragDrop::Payload()) return;

        float localY = (mouse.y - rowRect.y) / rowRect.height;
        TreeDropPosition pos;
        if (allowReparent && localY > 0.25f && localY < 0.75f) {
            pos = TreeDropPosition::Into;
            DrawRectangleLinesEx(rowRect, 2.0f, dropIntoColor);
        } else if (localY <= 0.5f) {
            pos = TreeDropPosition::Before;
            DrawRectangleRec({ rowRect.x, rowRect.y - 1.0f, rowRect.width, 2.0f }, dropLineColor);
        } else {
            pos = TreeDropPosition::After;
            DrawRectangleRec({ rowRect.x, rowRect.y + rowRect.height - 1.0f, rowRect.width, 2.0f }, dropLineColor);
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (onReorderOrReparent) onReorderOrReparent(DragDrop::Payload(), rowInfos[i].id, pos);
            DragDrop::Consume();
        }
        return;
    }
}
