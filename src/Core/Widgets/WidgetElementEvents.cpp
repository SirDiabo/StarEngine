#include "WidgetElementEvents.h"

namespace WidgetElementEvents {

static std::map<std::string, std::vector<std::string>>& Table() {
    static std::map<std::string, std::vector<std::string>> table;
    return table;
}

void Register(const std::string& elementClass, const std::vector<std::string>& eventNames) {
    Table()[elementClass] = eventNames;
}

const std::vector<std::string>* Find(const std::string& elementClass) {
    auto it = Table().find(elementClass);
    return it != Table().end() ? &it->second : nullptr;
}

const std::map<std::string, std::vector<std::string>>& GetAll() {
    return Table();
}

std::string Normalize(const std::string& name) {
    std::string out;
    for (char c : name) {
        if (c == ' ' || c == '_') continue;
        out += (char)tolower((unsigned char)c);
    }
    return out;
}

}
