#include "UndoManager.h"
#include <algorithm>

UndoManager& UndoManager::Get() {
    static UndoManager instance;
    return instance;
}

void UndoManager::RegisterDocument(UndoDocument* document) {
    if (!document) return;
    if (std::find(documents.begin(), documents.end(), document) != documents.end()) return;
    documents.push_back(document);
    ResetBaseline(document);
}

void UndoManager::UnregisterDocument(UndoDocument* document) {
    documents.erase(std::remove(documents.begin(), documents.end(), document), documents.end());
    baselines.erase(document);
    PurgeEntries(document);
}

void UndoManager::PurgeEntries(UndoDocument* document) {
    auto purge = [document](std::vector<std::unique_ptr<UndoEntry>>& stack) {
        stack.erase(std::remove_if(stack.begin(), stack.end(),
                        [document](const std::unique_ptr<UndoEntry>& entry) { return entry->References(document); }),
                    stack.end());
    };
    purge(undoStack);
    purge(redoStack);
}

void UndoManager::ForgetHistory(UndoDocument* document) {
    PurgeEntries(document);
    ResetBaseline(document);
}

void UndoManager::ResetBaseline(UndoDocument* document) {
    if (!document) return;
    auto snapshot = std::make_shared<Snapshot>();
    document->CaptureState(snapshot->content, snapshot->selection);
    baselines[document] = snapshot;
}

void UndoManager::PollDocument(UndoDocument* document) {
    if (!document->CanTrackChanges()) return;

    auto current = std::make_shared<Snapshot>();
    document->CaptureState(current->content, current->selection);

    auto it = baselines.find(document);
    if (it == baselines.end() || !it->second) {
        baselines[document] = current;
        return;
    }

    if (it->second->content == current->content) {
        if (it->second->selection != current->selection) baselines[document] = current;
        return;
    }

    std::shared_ptr<const Snapshot> before = it->second;
    std::unique_ptr<SnapshotUndoEntry> entry(new SnapshotUndoEntry(document, before, current));
    entry->label = document->UndoName();
    undoStack.push_back(std::move(entry));
    redoStack.clear();
    baselines[document] = current;
    Trim();
}

void UndoManager::Poll() {
    if (applying) return;
    std::vector<UndoDocument*> snapshotDocs = documents;
    for (UndoDocument* document : snapshotDocs) PollDocument(document);
}

void UndoManager::PushEntry(std::unique_ptr<UndoEntry> entry) {
    if (!entry || applying) return;
    undoStack.push_back(std::move(entry));
    redoStack.clear();
    Trim();
}

bool UndoManager::Undo() {
    Poll();
    while (!undoStack.empty()) {
        std::unique_ptr<UndoEntry> entry = std::move(undoStack.back());
        undoStack.pop_back();
        applying = true;
        bool ok = entry->Undo();
        applying = false;
        if (ok) {
            redoStack.push_back(std::move(entry));
            return true;
        }
    }
    return false;
}

bool UndoManager::Redo() {
    Poll();
    while (!redoStack.empty()) {
        std::unique_ptr<UndoEntry> entry = std::move(redoStack.back());
        redoStack.pop_back();
        applying = true;
        bool ok = entry->Redo();
        applying = false;
        if (ok) {
            undoStack.push_back(std::move(entry));
            return true;
        }
    }
    return false;
}

std::string UndoManager::UndoLabel() const {
    return undoStack.empty() ? std::string() : undoStack.back()->label;
}

std::string UndoManager::RedoLabel() const {
    return redoStack.empty() ? std::string() : redoStack.back()->label;
}

void UndoManager::Clear() {
    undoStack.clear();
    redoStack.clear();
    for (UndoDocument* document : documents) ResetBaseline(document);
}

void UndoManager::SetLimits(size_t entries, size_t bytes) {
    maxEntries = entries < 1 ? 1 : entries;
    maxBytes = bytes;
    Trim();
}

size_t UndoManager::TotalBytes() const {
    size_t total = 0;
    for (const auto& e : undoStack) total += e->ByteSize();
    for (const auto& e : redoStack) total += e->ByteSize();
    return total;
}

void UndoManager::Trim() {
    while (undoStack.size() > maxEntries) undoStack.erase(undoStack.begin());
    while (undoStack.size() > 1 && TotalBytes() > maxBytes) undoStack.erase(undoStack.begin());
}

SnapshotUndoEntry::SnapshotUndoEntry(UndoDocument* doc, std::shared_ptr<const UndoManager::Snapshot> b,
                                     std::shared_ptr<const UndoManager::Snapshot> a)
    : document(doc), before(std::move(b)), after(std::move(a)) {}

bool SnapshotUndoEntry::Undo() {
    document->RevealForUndo();
    document->RestoreState(before->content, before->selection);
    UndoManager::Get().ResetBaseline(document);
    return true;
}

bool SnapshotUndoEntry::Redo() {
    document->RevealForUndo();
    document->RestoreState(after->content, after->selection);
    UndoManager::Get().ResetBaseline(document);
    return true;
}

size_t SnapshotUndoEntry::ByteSize() const {
    return before->content.size() + after->content.size();
}

LambdaUndoEntry::LambdaUndoEntry(std::string entryLabel, std::function<bool()> undoFunction,
                                 std::function<bool()> redoFunction, size_t byteSize)
    : undoFn(std::move(undoFunction)), redoFn(std::move(redoFunction)), bytes(byteSize) {
    label = std::move(entryLabel);
}
