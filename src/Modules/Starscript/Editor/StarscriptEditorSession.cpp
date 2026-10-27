#include "StarscriptEditorSession.h"
#include "../StarscriptBuiltins.h"
#include "../StarscriptCompiler.h"
#include "../StarscriptInterface.h"
#include "Editor/EditorUndo.h"
#include "Editor/UITheme.h"
#include "Core/Widgets/WidgetInput.h"
#include "Runtime/ActorAsset.h"
#include "Runtime/ActorClassCache.h"
#include "Runtime/ComponentRegistry.h"
#include "Core/ObjectKindRegistry.h"
#include "raylib.h"
#include <algorithm>
#include <sstream>
#include <unordered_set>

using namespace Starscript;

StarscriptEditorSession::StarscriptEditorSession() {
    ActorClassEditorHost host;
    host.components = [this]() -> std::vector<ComponentDecl>& { return components; };
    host.markDirty = [this]() { isDirty = true; componentsRevision++; };
    host.onComponentSelected = []() {};
    actorEditor.SetHost(std::move(host));

    code.onTextChanged = [this]() { isDirty = true; compileTimer = 0.35f; };
    code.onSaveRequested = [this]() { if (isDirty) Save(); };
    code.completionProvider = [this](const CompletionRequest& request, std::vector<CompletionItem>& out) { BuildCompletions(request, out); };
    code.classifyIdentifier = [this](const std::string& name, bool afterDot, bool callFollows) { return Classify(name, afterDot, callFollows); };

    undoDocument = new FunctionUndoDocument();
    undoDocument->name = "Script Edit";
    undoDocument->capture = [this](std::string& content, std::string& selection) { CaptureUndoState(content, selection); };
    undoDocument->restore = [this](const std::string& content, const std::string& selection) { RestoreUndoState(content, selection); };
    undoDocument->reveal = [this]() { if (undoReveal) undoReveal(); };
    undoDocument->canTrack = [this]() {
        if (!isOpen) return false;
        return undoOwnerActive ? undoOwnerActive() : true;
    };
    UndoManager::Get().RegisterDocument(undoDocument);
}

StarscriptEditorSession::~StarscriptEditorSession() {
    if (undoDocument) {
        UndoManager::Get().UnregisterDocument(undoDocument);
        delete undoDocument;
        undoDocument = nullptr;
    }
}

void StarscriptEditorSession::SetUndoOwner(std::function<void()> reveal, std::function<bool()> isActive) {
    undoReveal = std::move(reveal);
    undoOwnerActive = std::move(isActive);
}

bool StarscriptEditorSession::Open(const std::string& path) {
    ActorAssetData data;
    if (!ActorAsset::Load(path, data)) {
        TraceLog(LOG_WARNING, "Starscript editor: failed to load \"%s\"", path.c_str());
        return false;
    }
    if (data.programmingInterface != kInterfaceName) {
        TraceLog(LOG_WARNING, "Starscript editor: \"%s\" is not a Starscript actor", path.c_str());
        return false;
    }

    filePath = path;
    assetName = data.assetName;
    parentClass = data.parentClass;
    components = data.components;

    std::string source;
    if (auto* content = dynamic_cast<StarscriptContent*>(data.behaviorContent)) source = content->source;
    code.SetText(source);
    code.GotoLine(0, 0);

    isOpen = true;
    isDirty = false;
    componentsRevision++;
    actorEditor.OnDocumentReplaced();
    actorEditor.ClearSelection();
    Compile();
    UndoManager::Get().ResetBaseline(undoDocument);
    return true;
}

void StarscriptEditorSession::Close() {
    isOpen = false;
    isDirty = false;
    filePath.clear();
    actorEditor.ResetPickers();
}

std::string StarscriptEditorSession::SerializeDocument() {
    ActorAssetData data;
    data.assetName = assetName;
    data.parentClass = parentClass;
    data.programmingInterface = kInterfaceName;
    data.components = components;
    auto* content = new StarscriptContent();
    content->source = code.GetText();
    data.behaviorContent = content;
    std::ostringstream out;
    ActorAsset::Write(out, data);
    return out.str();
}

bool StarscriptEditorSession::Save() {
    if (!isOpen) return false;
    ActorAssetData data;
    data.assetName = assetName;
    data.parentClass = parentClass;
    data.programmingInterface = kInterfaceName;
    data.components = components;
    auto* content = new StarscriptContent();
    content->source = code.GetText();
    data.behaviorContent = content;
    if (!ActorAsset::Save(filePath, data)) return false;
    isDirty = false;
    ActorClassCache::Get().Invalidate(filePath);
    return true;
}

void StarscriptEditorSession::CaptureUndoState(std::string& content, std::string& selection) {
    if (!isOpen) { content.clear(); selection.clear(); return; }
    if (undoCacheCodeVersion != code.GetVersion() || undoCacheComponentsRevision != componentsRevision) {
        undoCache = SerializeDocument();
        undoCacheCodeVersion = code.GetVersion();
        undoCacheComponentsRevision = componentsRevision;
    }
    content = undoCache;
    selection = code.GetViewState();
}

void StarscriptEditorSession::RestoreUndoState(const std::string& content, const std::string& selection) {
    if (!isOpen || content.empty()) return;
    ActorAssetData data;
    std::istringstream in(content);
    if (!ActorAsset::Read(in, data)) return;

    components = data.components;
    std::string source;
    if (auto* c = dynamic_cast<StarscriptContent*>(data.behaviorContent)) source = c->source;
    code.SetText(source);
    code.SetViewState(selection);
    componentsRevision++;
    isDirty = true;
    actorEditor.OnDocumentReplaced();
    undoCacheCodeVersion = -1;
    Compile();
}

void StarscriptEditorSession::Tick() {
    if (!isOpen) return;
    if (compileTimer > 0.0f) {
        compileTimer -= GetFrameTime();
        if (compileTimer <= 0.0f) Compile();
    }
    if (compiledComponentsRevision != componentsRevision) Compile();
}

void StarscriptEditorSession::Compile() {
    compileTimer = 0.0f;
    program = Starscript::Compile(code.GetText(), components);
    compiledCodeVersion = code.GetVersion();
    compiledComponentsRevision = componentsRevision;

    std::vector<CodeDiagnostic> diagnostics;
    for (const Diagnostic& d : program->diagnostics) {
        CodeDiagnostic cd;
        cd.line = std::max(0, d.line - 1);
        cd.column = std::max(0, d.column - 1);
        cd.message = d.message;
        diagnostics.push_back(cd);
    }
    code.SetDiagnostics(std::move(diagnostics));
}

std::string StarscriptEditorSession::UniqueName(const std::string& base) const {
    std::unordered_set<std::string> taken;
    if (program) {
        for (const auto& v : program->vars) taken.insert(v->name);
        for (const auto& f : program->funcs) taken.insert(f->name);
        for (const auto& e : program->events) taken.insert(e->name);
    }
    for (const ComponentDecl& c : components) taken.insert(c.name);
    if (!taken.count(base)) return base;
    for (int n = 2;; n++) {
        std::string candidate = base + "_" + std::to_string(n);
        if (!taken.count(candidate)) return candidate;
    }
}

void StarscriptEditorSession::AppendBlock(const std::string& text, int selectOffsetInBlock, int selectLength) {
    int lineCount = code.LineCount();
    bool endsEmpty = lineCount > 0 && code.LineText(lineCount - 1).empty();
    std::string prefix = endsEmpty ? "\n" : "\n\n";
    code.AppendText(prefix + text);
    int blockStartLine = code.LineCount() - 1;
    int newlines = (int)std::count(text.begin(), text.end(), '\n');
    blockStartLine = code.LineCount() - 1 - newlines;
    int line = blockStartLine, col = selectOffsetInBlock;
    code.GotoLine(line, col);
    code.SelectRange(line, col, selectLength);
}

TokenClass StarscriptEditorSession::Classify(const std::string& name, bool afterDot, bool callFollows) const {
    if (afterDot) return callFollows ? TokenClass::Function : TokenClass::Variable;
    if (FindBuiltin(name) || name == "Vec3" || name == "range") return TokenClass::Builtin;
    for (const ComponentDecl& c : components) if (c.name == name) return TokenClass::Component;
    if (program) {
        if (program->funcIndex.count(name)) return TokenClass::Function;
        for (const auto& e : program->events) if (e->kind == EventKind::Custom && e->name == name) return TokenClass::Function;
    }
    return TokenClass::Variable;
}

bool StarscriptEditorSession::ResolveReceiverKind(const std::string& receiver, int& outKind, std::string& outObjectKind) const {
    if (receiver.empty()) return false;
    std::vector<std::string> parts;
    std::stringstream ss(receiver);
    std::string part;
    while (std::getline(ss, part, '.')) parts.push_back(part);
    if (parts.empty()) return false;

    Kind kind = Kind::Void;
    std::string objectKind;
    if (parts[0] == "self") { kind = Kind::Actor; objectKind = "Actor"; }
    else {
        bool found = false;
        for (const ComponentDecl& c : components) if (c.name == parts[0]) { kind = Kind::Component; objectKind = c.componentType; found = true; break; }
        if (!found && program) {
            auto it = program->varIndex.find(parts[0]);
            if (it != program->varIndex.end()) {
                kind = program->vars[(size_t)it->second]->type.kind;
                objectKind = program->vars[(size_t)it->second]->type.objectKind;
                found = kind != Kind::Void;
            }
        }
        if (!found) return false;
    }
    for (size_t i = 1; i < parts.size(); i++) {
        const PropertyDef* prop = FindProperty(kind, parts[i], objectKind);
        if (!prop) return false;
        kind = prop->type.kind;
        objectKind = prop->type.objectKind;
    }
    outKind = (int)kind;
    outObjectKind = objectKind;
    return true;
}

static std::string SignatureOf(const std::vector<SType>& params, size_t minArgs, const SType& ret) {
    std::string s = "(";
    for (size_t i = 0; i < params.size(); i++) {
        if (i) s += ", ";
        s += params[i].ToString();
        if (i >= minArgs) s += "?";
    }
    s += ")";
    if (ret.kind != Kind::Void) s += " -> " + ret.ToString();
    return s;
}

void StarscriptEditorSession::BuildCompletions(const CompletionRequest& request, std::vector<CompletionItem>& out) {
    auto add = [&](const std::string& label, const std::string& insert, const std::string& detail, CompletionKind kind) {
        for (const CompletionItem& existing : out) if (existing.label == label) return;
        out.push_back({ label, insert, detail, kind });
    };

    if (request.afterDot) {
        int kindInt = 0;
        std::string objectKind;
        bool known = ResolveReceiverKind(request.receiver, kindInt, objectKind);
        Kind kind = (Kind)kindInt;
        auto typeMatches = [&](const char* required) {
            if (!required || !*required || !known || objectKind.empty()) return true;
            return objectKind == required || ObjectKindRegistry::Get().IsCompatible(objectKind, required);
        };
        if (known && kind == Kind::Vec3) {
            add("x", "x", "float", CompletionKind::Property);
            add("y", "y", "float", CompletionKind::Property);
            add("z", "z", "float", CompletionKind::Property);
        }
        if (known && kind == Kind::Component) {
            if (const ComponentTypeInfo* info = ComponentRegistry::Get().Find(objectKind)) {
                for (const ComponentFieldDesc& f : info->fields) {
                    if (f.hiddenInDetails) continue;
                    add(ComponentFieldIdentifier(f), ComponentFieldIdentifier(f), ComponentFieldScriptType(f).ToString() + " (field)", CompletionKind::Property);
                }
            }
        }
        for (const PropertyDef& p : AllProperties()) {
            if (known && (p.receiver != kind || !typeMatches(p.componentType))) continue;
            add(p.name, p.name, p.type.ToString() + (p.set ? "" : " (read-only)"), CompletionKind::Property);
        }
        for (const MethodDef& m : AllMethods()) {
            if (known && (m.receiver != kind || !typeMatches(m.componentType))) continue;
            std::string insert = std::string(m.name) + (m.params.empty() ? "()" : "(|)");
            add(m.name, insert, SignatureOf(m.params, m.minArgs, m.ret), CompletionKind::Method);
        }
        if (!known) {
            add("x", "x", "float", CompletionKind::Property);
            add("y", "y", "float", CompletionKind::Property);
            add("z", "z", "float", CompletionKind::Property);
        }
        return;
    }

    static const char* keywords[] = { "var", "func", "event", "if", "elif", "else", "while", "for", "in", "return", "break", "continue", "pass", "and", "or", "not", "true", "false", "self" };
    for (const char* k : keywords) add(k, k, "", CompletionKind::Keyword);

    static const char* types[] = { "bool", "int", "float", "string", "Vec3", "Actor", "Array[float]", "Array[int]", "Array[string]" };
    for (const char* t : types) add(t, t, "type", CompletionKind::Type);

    static const char* annotations[] = { "@replicated", "@server_only", "@save(\"|\")", "@reliable" };
    for (const char* a : annotations) {
        std::string label = a;
        size_t bar = label.find('|');
        if (bar != std::string::npos) label = "@save";
        add(label, a, "annotation", CompletionKind::Snippet);
    }

    add("range", "range(|)", "(int, int?, int?) for loops", CompletionKind::Function);
    add("Vec3", "Vec3(|)", "(float, float, float) -> Vec3", CompletionKind::Function);
    for (const BuiltinDef& b : AllBuiltins()) {
        std::string insert = std::string(b.name) + (b.params.empty() ? "()" : "(|)");
        add(b.name, insert, SignatureOf(b.params, b.minArgs, b.ret), CompletionKind::Function);
    }

    add("Input", "Input(\"|\"):", "event (key or input mapping)", CompletionKind::Event);
    add("await", "await ", "keyword", CompletionKind::Keyword);
    add("BeginPlay", "BeginPlay():", "event", CompletionKind::Event);
    add("Tick", "Tick(dt: float):", "event", CompletionKind::Event);
    add("EndPlay", "EndPlay():", "event", CompletionKind::Event);
    add("Destroyed", "Destroyed():", "event", CompletionKind::Event);

    for (const ComponentDecl& c : components) add(c.name, c.name, c.componentType, CompletionKind::Component);

    if (program) {
        for (const auto& v : program->vars) add(v->name, v->name, v->type.ToString(), CompletionKind::Variable);
        for (const auto& f : program->funcs) {
            std::string sig = "(";
            for (size_t i = 0; i < f->params.size(); i++) { if (i) sig += ", "; sig += f->params[i].name + ": " + f->params[i].type.ToString(); }
            sig += ")";
            if (f->ret.kind != Kind::Void) sig += " -> " + f->ret.ToString();
            add(f->name, f->name + (f->params.empty() ? "()" : "(|)"), sig, CompletionKind::Function);
        }
        for (const auto& e : program->events) {
            if (e->kind != EventKind::Custom) continue;
            add(e->name, e->name + (e->params.empty() ? "()" : "(|)"), "event", CompletionKind::Event);
        }
    }
}

void StarscriptEditorSession::RenderToolbar(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    Rectangle toolbar = { area.x, area.y, area.width, GetToolbarHeight() };
    DrawRectangleRec(toolbar, theme.tabBar);
    DrawLine((int)toolbar.x, (int)(toolbar.y + toolbar.height), (int)(toolbar.x + toolbar.width), (int)(toolbar.y + toolbar.height), theme.panelBorder);

    float btnY = toolbar.y + 4;
    float btnLeft = area.x + 8;

    Rectangle saveBtn = { btnLeft, btnY, 50, 22 };
    bool saveHov = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), saveBtn);
    DrawRectangleRec(saveBtn, saveHov ? theme.buttonHover : theme.buttonBg);
    DrawRectangleLinesEx(saveBtn, 1, theme.buttonBorder);
    int saveW = MeasureText("Save", 11);
    DrawText("Save", (int)(saveBtn.x + (saveBtn.width - saveW) / 2.0f), (int)(saveBtn.y + (saveBtn.height - 11) / 2.0f), 11, theme.textPrimary);
    if (saveHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) Save();

    int errors = ErrorCount();
    std::string status = errors == 0 ? "No problems" : std::to_string(errors) + (errors == 1 ? " problem" : " problems");
    Color statusColor = errors == 0 ? theme.success : theme.danger;
    DrawText(status.c_str(), (int)(saveBtn.x + saveBtn.width + 14), (int)(btnY + 5), 12, statusColor);

    Rectangle closeBtn = { area.x + area.width - 26, btnY, 22, 22 };
    bool closeHov = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), closeBtn);
    DrawRectangleRec(closeBtn, closeHov ? theme.dangerBg : theme.buttonBg);
    DrawRectangleLinesEx(closeBtn, 1, theme.buttonBorder);
    int closeW = MeasureText("X", 11);
    DrawText("X", (int)(closeBtn.x + (closeBtn.width - closeW) / 2.0f), (int)(closeBtn.y + (closeBtn.height - 11) / 2.0f), 11, theme.textPrimary);
    if (closeHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (isDirty) Save();
        Close();
    }
}
