#pragma once
#include <string>
#include <vector>

enum class PrintMessageType { Info, Warning, Error };

class PrintMessageLog {
public:
    static PrintMessageLog& Get();

    void Add(PrintMessageType type, const std::string& text, float duration);
    void Tick(float deltaTime);
    void Draw();
    void Clear();

private:
    PrintMessageLog() = default;

    struct Message {
        PrintMessageType type;
        std::string text;
        float remaining;
    };

    std::vector<Message> messages;
};
