#pragma once
#include "Core/UndoManager.h"
#include <functional>
#include <string>

class EditorTab;

namespace EditorUndo {

void Tick();
bool PerformUndo();
bool PerformRedo();

void SetTabRevealer(std::function<void(EditorTab*)> revealer);
void RevealTab(EditorTab* tab);
void SetActiveTabQuery(std::function<EditorTab*()> query);
bool IsTabActive(const EditorTab* tab);

}

class FunctionUndoDocument : public UndoDocument {
public:
    std::string name;
    EditorTab* tab = nullptr;
    std::function<void(std::string&, std::string&)> capture;
    std::function<void(const std::string&, const std::string&)> restore;
    std::function<bool()> canTrack;
    std::function<void()> reveal;

    std::string UndoName() const override { return name; }
    bool CanTrackChanges() const override {
        if (tab && !EditorUndo::IsTabActive(tab)) return false;
        return canTrack ? canTrack() : true;
    }
    void CaptureState(std::string& content, std::string& selection) override {
        if (capture) capture(content, selection);
    }
    void RestoreState(const std::string& content, const std::string& selection) override {
        if (restore) restore(content, selection);
    }
    void RevealForUndo() override {
        if (reveal) reveal();
        else EditorUndo::RevealTab(tab);
    }
};
