#include "ComponentEvents.h"
#include "Actor.h"
#include "ActorBehavior.h"
#include "Components/Component.h"

ComponentEventRegistry& ComponentEventRegistry::Get() {
    static ComponentEventRegistry instance;
    return instance;
}

void ComponentEventRegistry::Register(ComponentEventDecl decl) {
    events.push_back(std::move(decl));
}

namespace ComponentEvents {

void Raise(Component* component, const std::string& eventName,
           const std::vector<std::pair<std::string, std::string>>& params) {
    if (!component) return;
    Actor* owner = component->GetOwningActor();
    if (!owner) return;
    ActorBehavior* behavior = owner->GetBehavior();
    if (!behavior) return;
    behavior->OnComponentEvent(owner, component, eventName, params);
}

}
