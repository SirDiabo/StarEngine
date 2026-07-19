#include "PrintMessageLog.h"
#include "raylib.h"
#include <algorithm>

static const size_t kMaxMessages = 200;

PrintMessageLog& PrintMessageLog::Get() {
    static PrintMessageLog instance;
    return instance;
}

void PrintMessageLog::Add(PrintMessageType type, const std::string& text, float duration) {
    if (messages.size() >= kMaxMessages) messages.erase(messages.begin());
    messages.push_back({type, text, duration});
}

void PrintMessageLog::Tick(float deltaTime) {
    for (Message& m : messages) m.remaining -= deltaTime;

    messages.erase(std::remove_if(messages.begin(), messages.end(), [](const Message& m) {
        return m.remaining <= 0.0f;
    }), messages.end());
}

void PrintMessageLog::Draw() {
    int y = 10;
    for (const Message& m : messages) {
        Color color = WHITE;
        if (m.type == PrintMessageType::Warning) color = YELLOW;
        if (m.type == PrintMessageType::Error)   color = RED;
        DrawText(m.text.c_str(), 10, y, 12, color);
        y += 24;
    }
}

void PrintMessageLog::Clear() {
    messages.clear();
}