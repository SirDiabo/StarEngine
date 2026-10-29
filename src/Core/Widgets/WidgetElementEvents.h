#pragma once
#include <map>
#include <string>
#include <vector>

namespace WidgetElementEvents {

void Register(const std::string& elementClass, const std::vector<std::string>& eventNames);
const std::vector<std::string>* Find(const std::string& elementClass);
const std::map<std::string, std::vector<std::string>>& GetAll();
std::string Normalize(const std::string& name);

}

struct WidgetElementEventRegistrar {
    WidgetElementEventRegistrar(const std::string& elementClass, const std::vector<std::string>& eventNames) {
        WidgetElementEvents::Register(elementClass, eventNames);
    }
};
