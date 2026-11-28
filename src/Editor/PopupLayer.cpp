#include "PopupLayer.h"
#include <algorithm>
#include <utility>
#include <vector>

namespace {

struct Entry {
    const void* owner;
    std::function<void()> render;
};

std::vector<Entry>& Entries() {
    static std::vector<Entry> entries;
    return entries;
}

}

namespace PopupLayer {

void Add(const void* owner, std::function<void()> render) {
    for (Entry& entry : Entries()) {
        if (entry.owner == owner) {
            entry.render = std::move(render);
            return;
        }
    }
    Entries().push_back({ owner, std::move(render) });
}

void Remove(const void* owner) {
    std::vector<Entry>& entries = Entries();
    entries.erase(std::remove_if(entries.begin(), entries.end(), [owner](const Entry& e) { return e.owner == owner; }), entries.end());
}

void RenderAll() {
    std::vector<Entry> snapshot = Entries();
    for (const Entry& entry : snapshot) {
        bool stillRegistered = false;
        for (const Entry& live : Entries()) if (live.owner == entry.owner) { stillRegistered = true; break; }
        if (stillRegistered && entry.render) entry.render();
    }
}

}
