#pragma once
#include "CodeEditor.h"
#include "Editor/ActorClassEditor.h"
#include "../StarscriptAst.h"
#include "Runtime/ComponentDecl.h"
#include "Core/ProgrammingInterfaceRegistry.h"
#include "../StarscriptCompiler.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class FunctionUndoDocument;

namespace Starscript { class StarscriptContent; }

class StarscriptEditorSession {
public:
    StarscriptEditorSession();
    ~StarscriptEditorSession();

    bool Open(const std::string& path);
    bool OpenEmbeddedWidget(Starscript::StarscriptContent* content, const ProgrammingInterfaceContext& context);
    void SyncToContent();
    bool IsWidgetMode() const { return widgetMode; }
    void BeginElementDrag(const std::string& elementName, const std::string& elementClass);
    bool IsElementDragActive() const { return elementDragActive; }
    const std::string& ElementDragName() const { return elementDragName; }
    void EndElementDrag() { elementDragActive = false; }
    void InsertElementReference(Vector2 point);
    void Close();
    bool IsOpen() const { return isOpen; }
    bool IsDirty() const { return isDirty; }
    bool Save();

    void SetUndoOwner(std::function<void()> reveal, std::function<bool()> isActive);
    void Tick();

    void RenderToolbar(Rectangle area);
    static float GetToolbarHeight() { return 30.0f; }

    void SetScreenBounds(Rectangle b) { screenBounds = b; }
    Rectangle GetScreenBounds() const { return screenBounds; }

    CodeEditor& GetCode() { return code; }
    ActorClassEditor& GetActorEditor() { return actorEditor; }
    const std::string& GetFilePath() const { return filePath; }
    const Starscript::Program* GetProgram() const { return program.get(); }
    int ErrorCount() const { return program ? (int)program->diagnostics.size() : 0; }

    void AppendBlock(const std::string& text, int selectOffsetInBlock, int selectLength);
    std::string UniqueName(const std::string& base) const;

private:
    void Compile();
    void RefreshElementList();
    void NotifyDirty();
    void CaptureUndoState(std::string& content, std::string& selection);
    void RestoreUndoState(const std::string& content, const std::string& selection);
    std::string SerializeDocument();
    void BuildCompletions(const CompletionRequest& request, std::vector<CompletionItem>& out);
    TokenClass Classify(const std::string& name, bool afterDot, bool callFollows) const;
    bool ResolveReceiverKind(const std::string& receiver, int& outKind, std::string& outObjectKind) const;

    std::string filePath;
    std::string assetName;
    std::string parentClass = "Actor";
    std::vector<ComponentDecl> components;
    CodeEditor code;
    ActorClassEditor actorEditor;

    bool isOpen = false;
    bool isDirty = false;
    int componentsRevision = 0;

    std::shared_ptr<Starscript::Program> program;
    int compiledCodeVersion = -1;
    int compiledComponentsRevision = -1;
    float compileTimer = 0.0f;

    bool widgetMode = false;
    Starscript::StarscriptContent* widgetContent = nullptr;
    std::function<std::vector<std::pair<std::string, std::string>>()> listElements;
    std::function<void()> onContentDirtied;
    std::function<void()> onSaveRequestedHook;
    std::vector<Starscript::WidgetElementInfo> elements;
    std::string elementSignature;
    float elementPollTimer = 0.0f;
    bool elementDragActive = false;
    std::string elementDragName;
    std::string elementDragClass;

    FunctionUndoDocument* undoDocument = nullptr;
    std::function<void()> undoReveal;
    std::function<bool()> undoOwnerActive;
    std::string undoCache;
    int undoCacheCodeVersion = -1;
    int undoCacheComponentsRevision = -1;

    Rectangle screenBounds{ 0, 0, 0, 0 };
};
