#pragma once
#include "raylib.h"
#include "Runtime/ComponentDecl.h"
#include "Editor/ComponentEditing.h"
#include "Editor/PropertyPanel.h"
#include "Editor/SearchableListPopup.h"
#include "Editor/RegistryPickerPopup.h"
#include "Editor/GizmoRenderer.h"
#include "Core/Widgets/TreeListElement.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class Widget;
class VerticalBoxElement;
class TextElement;

template <typename T>
inline void MoveInList(std::vector<T>& items, int fromIndex, int toIndex, TreeDropPosition pos) {
    if (fromIndex < 0 || fromIndex >= (int)items.size()) return;
    if (toIndex < 0 || toIndex >= (int)items.size()) return;
    if (fromIndex == toIndex) return;

    T moved = items[fromIndex];
    items.erase(items.begin() + fromIndex);

    int insertAt = toIndex;
    if (fromIndex < toIndex) insertAt--;
    if (pos == TreeDropPosition::After) insertAt++;
    if (insertAt < 0) insertAt = 0;
    if (insertAt > (int)items.size()) insertAt = (int)items.size();

    items.insert(items.begin() + insertAt, moved);
}

struct ActorClassEditorHost {
    std::function<std::vector<ComponentDecl>&()> components;
    std::function<void()> markDirty;
    std::function<void()> onComponentSelected;
};

class ActorClassEditor {
public:
    ActorClassEditor();
    ~ActorClassEditor();
    ActorClassEditor(const ActorClassEditor&) = delete;
    ActorClassEditor& operator=(const ActorClassEditor&) = delete;

    void SetHost(ActorClassEditorHost newHost);

    int GetSelectedIndex() const { return selectedComponentIndex; }
    int GetDetailIndex() const { return lastValidComponentIndex; }
    void SelectComponent(int index);
    void ClearSelection();
    void ClampSelection();
    void OnDocumentReplaced();

    void RenderOutliner(Rectangle area, Rectangle screenBounds);

    bool HasDetailsSelection();
    std::string DetailsSelectionKey();
    void BuildDetailsRows(std::vector<PropertyRow>& rows);
    bool OnAssetPicked(const std::string& rowId, const std::string& pickedPath);
    void RenderPickers(Rectangle panel);
    void ResetPickers();

    void RenderPreview(Rectangle area);
    void ResetGizmoHover() { previewGizmoState.hoverAxis = -1; }
    void SetPreviewFullscreen(bool v) { previewFullscreen = v; }
    void SetOnTogglePreviewFullscreen(std::function<void()> cb) { onTogglePreviewFullscreen = std::move(cb); }

private:
    std::vector<ComponentDecl>& Components();
    void MarkDirty();
    void NotifyComponentSelected();

    void BuildOutlinerChrome();
    void BuildComponentRows(std::vector<PropertyRow>& rows, int index);
    ComponentDecl* DetailComponent(int index);
    void SetComponentField(int index, const std::string& fieldKey, const std::string& value);

    void UpdatePreviewCamera(Rectangle client);
    void RenderPreviewScene();
    void HandlePreviewInteraction(Rectangle client);
    void RenderPreviewToolbar(Rectangle toolbarArea);
    void EnsurePreviewRenderTarget(int width, int height);
    Texture2D GetOrLoadPreviewIcon(const std::string& iconFile);

    ActorClassEditorHost host;
    LiveComponentCache liveComponents;

    int selectedComponentIndex = -1;
    int lastValidComponentIndex = -1;

    Widget* compChrome = nullptr;
    VerticalBoxElement* compRoot = nullptr;
    TreeListElement* compList = nullptr;
    TextElement* compEmptyText = nullptr;
    SearchableListPopup addComponentMenu;

    RegistryPickerPopup savePicker;
    std::string saveTargetFieldKey;
    int saveTargetIndex = -1;

    RegistryPickerPopup inputMappingPicker;
    SearchableListPopup inputKeyPicker;
    SearchableListPopup inputDevicePicker;
    std::string activeDeviceFieldKey;
    int inputKeyPickerMappingIndex = -1;
    int inputKeyPickerBindingIndex = -1;
    int listeningForInputBindingMappingIndex = -1;
    int listeningForInputBindingIndex = -1;

    Camera3D previewCamera{{0, 0, 0}, {0, 0, 0}, {0, 1, 0}, 50.0f, CAMERA_PERSPECTIVE};
    RenderTexture2D previewRenderTarget{};
    int previewRenderTargetWidth = 0;
    int previewRenderTargetHeight = 0;

    bool previewFlying = false;
    bool previewOrbiting = false;
    Vector2 previewLookStart{0, 0};
    Vector2 previewOrbitStart{0, 0};
    Vector3 previewFlyPosition{0, 0, 0};
    float previewYaw = 45.0f;
    float previewPitch = 35.0f;
    float previewOrbitRadius = 400.0f;
    float previewFlySpeed = 400.0f;
    Vector3 previewCameraTarget{0, 0, 0};

    std::unordered_map<std::string, Texture2D> previewIcons;

    GizmoMode previewGizmoMode = GizmoMode::Select;
    GizmoSpace previewGizmoSpace = GizmoSpace::World;
    GizmoDragState previewGizmoState;

    bool previewFullscreen = false;
    std::function<void()> onTogglePreviewFullscreen;

    static const float previewToolbarHeight;
};
