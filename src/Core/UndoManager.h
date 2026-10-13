#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class UndoDocument {
public:
    virtual ~UndoDocument() = default;

    virtual std::string UndoName() const = 0;
    virtual bool CanTrackChanges() const { return true; }
    virtual void CaptureState(std::string& content, std::string& selection) = 0;
    virtual void RestoreState(const std::string& content, const std::string& selection) = 0;
    virtual void RevealForUndo() {}
};

class UndoEntry {
public:
    virtual ~UndoEntry() = default;

    virtual bool Undo() = 0;
    virtual bool Redo() = 0;
    virtual size_t ByteSize() const { return 0; }
    virtual bool References(const UndoDocument* document) const { return false; }

    std::string label;
};

class UndoManager {
public:
    static UndoManager& Get();

    void RegisterDocument(UndoDocument* document);
    void UnregisterDocument(UndoDocument* document);
    void ResetBaseline(UndoDocument* document);
    void ForgetHistory(UndoDocument* document);

    void Poll();
    void PushEntry(std::unique_ptr<UndoEntry> entry);

    bool Undo();
    bool Redo();
    bool CanUndo() const { return !undoStack.empty(); }
    bool CanRedo() const { return !redoStack.empty(); }
    std::string UndoLabel() const;
    std::string RedoLabel() const;

    void Clear();
    bool IsApplying() const { return applying; }

    void SetLimits(size_t maxEntries, size_t maxBytes);
    size_t UndoDepth() const { return undoStack.size(); }
    size_t RedoDepth() const { return redoStack.size(); }
    size_t DocumentCount() const { return documents.size(); }

    struct Snapshot {
        std::string content;
        std::string selection;
    };

private:
    UndoManager() = default;

    void Trim();
    void PurgeEntries(UndoDocument* document);
    size_t TotalBytes() const;
    void PollDocument(UndoDocument* document);

    std::vector<UndoDocument*> documents;
    std::unordered_map<UndoDocument*, std::shared_ptr<const Snapshot>> baselines;
    std::vector<std::unique_ptr<UndoEntry>> undoStack;
    std::vector<std::unique_ptr<UndoEntry>> redoStack;
    size_t maxEntries = 1000;
    size_t maxBytes = 512u * 1024u * 1024u;
    bool applying = false;
};

class SnapshotUndoEntry : public UndoEntry {
public:
    SnapshotUndoEntry(UndoDocument* document, std::shared_ptr<const UndoManager::Snapshot> before,
                      std::shared_ptr<const UndoManager::Snapshot> after);

    bool Undo() override;
    bool Redo() override;
    size_t ByteSize() const override;
    bool References(const UndoDocument* candidate) const override { return candidate == document; }

private:
    UndoDocument* document;
    std::shared_ptr<const UndoManager::Snapshot> before;
    std::shared_ptr<const UndoManager::Snapshot> after;
};

class LambdaUndoEntry : public UndoEntry {
public:
    LambdaUndoEntry(std::string entryLabel, std::function<bool()> undoFn, std::function<bool()> redoFn,
                    size_t bytes = 0);

    bool Undo() override { return undoFn ? undoFn() : false; }
    bool Redo() override { return redoFn ? redoFn() : false; }
    size_t ByteSize() const override { return bytes; }

private:
    std::function<bool()> undoFn;
    std::function<bool()> redoFn;
    size_t bytes;
};
