#pragma once
#include <string>
#include <utility>
#include <vector>

class Component;

enum class EventValueType { Bool, Int, Float, String, Vec3, Rotator, Object };

struct ComponentEventParam {
    std::string name;
    EventValueType type = EventValueType::String;
    std::string objectKind;
};

struct ComponentEventDecl {
    std::string componentType;
    std::string name;
    std::vector<ComponentEventParam> params;
};

class ComponentEventRegistry {
public:
    static ComponentEventRegistry& Get();

    void Register(ComponentEventDecl decl);
    const std::vector<ComponentEventDecl>& GetAll() const { return events; }

private:
    std::vector<ComponentEventDecl> events;
};

struct ComponentEventRegistrar {
    ComponentEventRegistrar(ComponentEventDecl decl) { ComponentEventRegistry::Get().Register(std::move(decl)); }
};

namespace ComponentEvents {

void Raise(Component* component, const std::string& eventName,
           const std::vector<std::pair<std::string, std::string>>& params = {});

}
