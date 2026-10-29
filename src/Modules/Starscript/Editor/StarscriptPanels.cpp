#include "StarscriptPanels.h"
#include "StarscriptEditorSession.h"
#include "Editor/UITheme.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/WidgetFont.h"
#include <algorithm>
#include <string>

using namespace Starscript;

namespace {

constexpr float kStatusHeight = 24.0f;

bool Hover(Rectangle r) {
    const void* scope = WidgetInput::GetInputScope();
    Vector2 m = GetMousePosition();
    if (WidgetInput::IsMouseBlocked()) return false;
    if (scope && WidgetInput::IsMouseOccluded(m, scope)) return false;
    return CheckCollisionPointRec(m, r);
}

}

void StarscriptScriptPanel::Initialize() {
    name = "Script";
}

void StarscriptScriptPanel::Render() {
    if (!session || !session->IsOpen()) return;
    const UIPalette& theme = UITheme::Get();
    Font font = WidgetFont::Get();

    session->Tick();

    Rectangle codeArea = { bounds.x, bounds.y, bounds.width, bounds.height - kStatusHeight };
    session->GetCode().Render(codeArea);

    if (session->IsElementDragActive()) {
        Vector2 mp = GetMousePosition();
        std::string label = "Insert: " + session->ElementDragName();
        float w = MeasureTextEx(font, label.c_str(), 12, 1.2f).x + 14;
        Rectangle ghost = { mp.x + 12, mp.y - 4, w, 20 };
        DrawRectangleRec(ghost, theme.buttonBg);
        DrawRectangleLinesEx(ghost, 1, theme.buttonBorder);
        DrawTextEx(font, label.c_str(), { ghost.x + 7, ghost.y + 4 }, 12, 1.2f, theme.textPrimary);
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mp, codeArea)) session->InsertElementReference(mp);
            session->EndElementDrag();
        }
    }

    Rectangle status = { bounds.x, bounds.y + bounds.height - kStatusHeight, bounds.width, kStatusHeight };
    DrawRectangleRec(status, theme.tabBar);
    DrawLine((int)status.x, (int)status.y, (int)(status.x + status.width), (int)status.y, theme.panelBorder);

    CodeEditor& code = session->GetCode();
    std::string position = "Ln " + std::to_string(code.CaretLine() + 1) + ", Col " + std::to_string(code.CaretColumn() + 1);
    if (code.SelectionLength() > 0) position += "  (" + std::to_string(code.SelectionLength()) + " selected)";
    DrawTextEx(font, position.c_str(), { status.x + 10, status.y + 5 }, 13, 1.3f, theme.textSecondary);

    const std::vector<CodeDiagnostic>& diags = code.GetDiagnostics();
    if (!diags.empty()) {
        const CodeDiagnostic* shown = &diags[0];
        for (const CodeDiagnostic& d : diags) {
            if (d.line == code.CaretLine()) { shown = &d; break; }
        }
        std::string text = "Line " + std::to_string(shown->line + 1) + ": " + shown->message;
        if (diags.size() > 1) text += "   (+" + std::to_string(diags.size() - 1) + " more)";

        float posW = MeasureTextEx(font, position.c_str(), 13, 1.3f).x;
        Rectangle msg = { status.x + 10 + posW + 24, status.y, status.width - posW - 60, status.height };
        if (msg.width > 40) {
            BeginScissorMode((int)msg.x, (int)msg.y, (int)msg.width, (int)msg.height);
            bool hov = Hover(msg);
            DrawTextEx(font, text.c_str(), { msg.x, msg.y + 5 }, 13, 1.3f, hov ? Color{ 255, 140, 140, 255 } : theme.danger);
            EndScissorMode();
            if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) code.GotoLine(shown->line, shown->column);
        }
    }

    const char* label = "Starscript";
    float lw = MeasureTextEx(font, label, 13, 1.3f).x;
    DrawTextEx(font, label, { status.x + status.width - lw - 10, status.y + 5 }, 13, 1.3f, theme.textSecondary);
}

void StarscriptOutlinePanel::Initialize() {
    name = "Outline";
}

void StarscriptOutlinePanel::Render() {
    if (!session || !session->IsOpen()) return;
    const UIPalette& theme = UITheme::Get();
    Font font = WidgetFont::Get();

    DrawRectangleRec(bounds, theme.panelBg);

    const float buttonRowH = 30.0f;
    const float rowH = 22.0f;
    Rectangle buttons = { bounds.x, bounds.y, bounds.width, buttonRowH };
    DrawRectangleRec(buttons, theme.tabBar);
    DrawLine((int)buttons.x, (int)(buttons.y + buttons.height), (int)(buttons.x + buttons.width), (int)(buttons.y + buttons.height), theme.panelBorder);

    struct AddButton { const char* label; int kind; };
    AddButton add[] = { { "+ Var", 0 }, { "+ Func", 1 }, { "+ Event", 2 } };
    float bx = buttons.x + 6;
    for (const AddButton& b : add) {
        float w = MeasureTextEx(font, b.label, 12, 1.2f).x + 16;
        Rectangle r = { bx, buttons.y + 5, w, 20 };
        bool hov = Hover(r);
        DrawRectangleRec(r, hov ? theme.buttonHover : theme.buttonBg);
        DrawRectangleLinesEx(r, 1, theme.buttonBorder);
        DrawTextEx(font, b.label, { r.x + 8, r.y + 4 }, 12, 1.2f, theme.textPrimary);
        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (b.kind == 0) {
                std::string n = session->UniqueName("new_variable");
                session->AppendBlock("var " + n + " := 0\n", 4, (int)n.size());
            } else if (b.kind == 1) {
                std::string n = session->UniqueName("new_function");
                session->AppendBlock("func " + n + "():\n    pass\n", 5, (int)n.size());
            } else {
                std::string n = session->UniqueName("NewEvent");
                session->AppendBlock("event " + n + "():\n    pass\n", 6, (int)n.size());
            }
        }
        bx += w + 6;
    }

    Rectangle list = { bounds.x, bounds.y + buttonRowH + 1, bounds.width, bounds.height - buttonRowH - 1 };

    struct Row { std::string text; std::string tag; int line; bool header; Color color; };
    std::vector<Row> rows;
    const Program* program = session->GetProgram();
    if (program) {
        rows.push_back({ "Variables", "", -1, true, theme.sectionHeader });
        for (const auto& v : program->vars) {
            std::string tag;
            if (v->replication == ReplicationMode::Replicated) tag = "replicated";
            else if (v->replication == ReplicationMode::ServerOnly) tag = "server only";
            if (!v->saveFile.empty()) tag += (tag.empty() ? "" : ", ") + std::string("saved");
            rows.push_back({ v->name + ": " + v->type.ToString(), tag, v->line - 1, false, theme.textPrimary });
        }
        rows.push_back({ "Functions", "", -1, true, theme.sectionHeader });
        for (const auto& f : program->funcs) {
            std::string sig = f->name + "(";
            for (size_t i = 0; i < f->params.size(); i++) { if (i) sig += ", "; sig += f->params[i].name; }
            sig += ")";
            std::string tag = f->ret.kind == Kind::Void ? "" : "-> " + f->ret.ToString();
            rows.push_back({ sig, tag, f->line - 1, false, theme.textPrimary });
        }
        rows.push_back({ "Events", "", -1, true, theme.sectionHeader });
        for (const auto& e : program->events) {
            std::string label = (e->kind == EventKind::Component || e->kind == EventKind::Element) ? e->componentName + "." + e->name : e->name;
            if (e->kind == EventKind::Input) label = "Input(\"" + e->inputName + "\"";
            else label += "(";
            if (e->kind == EventKind::Input && !e->params.empty()) label += ", ";
            for (size_t i = 0; i < e->params.size(); i++) { if (i) label += ", "; label += e->params[i].name; }
            label += ")";
            std::string tag;
            if (e->replication == ReplicationMode::Replicated) tag = "replicated";
            else if (e->replication == ReplicationMode::ServerOnly) tag = "server only";
            rows.push_back({ label, tag, e->line - 1, false, theme.textPrimary });
        }
    }

    float contentH = rows.size() * rowH + 8.0f;
    if (Hover(list)) scroll -= GetMouseWheelMove() * rowH * 2.0f;
    scroll = std::max(0.0f, std::min(scroll, std::max(0.0f, contentH - list.height)));

    BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    int currentLine = session->GetCode().CaretLine();
    float y = list.y + 4 - scroll;
    for (size_t i = 0; i < rows.size(); i++) {
        const Row& row = rows[i];
        Rectangle r = { list.x, y, list.width, rowH };
        y += rowH;
        if (r.y + r.height < list.y || r.y > list.y + list.height) continue;

        if (row.header) {
            DrawTextEx(font, row.text.c_str(), { r.x + 10, r.y + 4 }, 12, 1.2f, row.color);
            continue;
        }

        bool hov = Hover(r) && Hover(list);
        bool active = row.line == currentLine;
        if (active) DrawRectangleRec(r, theme.surfaceSelected);
        else if (hov) DrawRectangleRec(r, theme.surfaceHover);

        float tagW = row.tag.empty() ? 0.0f : MeasureTextEx(font, row.tag.c_str(), 11, 1.1f).x + 8;
        BeginScissorMode((int)r.x, (int)r.y, (int)std::max(10.0f, r.width - tagW - 6), (int)r.height);
        DrawTextEx(font, row.text.c_str(), { r.x + 18, r.y + 4 }, 13, 1.3f, row.color);
        EndScissorMode();
        BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
        if (!row.tag.empty()) DrawTextEx(font, row.tag.c_str(), { r.x + r.width - tagW, r.y + 5 }, 11, 1.1f, theme.textSecondary);

        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) session->GetCode().GotoLine(row.line, 0);
    }
    EndScissorMode();
}
