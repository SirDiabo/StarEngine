#include "BillboardComponent.h"
#include "Modules/Stargraph/ComponentRegistry.h"
#include "Core/SafeParse.h"

void BillboardComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddString("Texture", &texturePath);
    outProperties.AddFloat("Width", &width);
    outProperties.AddFloat("Height", &height);
    outProperties.AddBool("Hidden In Game", &hiddenInGame);
}

void BillboardComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}

namespace {
ComponentRegistrar registrar_BillboardComponent({
    "BillboardComponent",
    []() -> Component* { return new BillboardComponent(); },
    {
        ComponentField::Asset("BillboardTexture", "Texture", ".startex",
            [](Component& c, const ComponentDecl& d) { static_cast<BillboardComponent&>(c).texturePath = d.Get("BillboardTexture"); }, [](const Component& c) { return static_cast<const BillboardComponent&>(c).texturePath; }),
        ComponentField::Flt("BillboardWidth", "Width", 50.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<BillboardComponent&>(c).width = SafeParse::ToFloat(d.Get("BillboardWidth", "50")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const BillboardComponent&>(c).width); }),
        ComponentField::Flt("BillboardHeight", "Height", 50.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<BillboardComponent&>(c).height = SafeParse::ToFloat(d.Get("BillboardHeight", "50")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const BillboardComponent&>(c).height); }),
        ComponentField::Bln("BillboardHiddenInGame", "Hidden In Game", true,
            [](Component& c, const ComponentDecl& d) { static_cast<BillboardComponent&>(c).hiddenInGame = d.Get("BillboardHiddenInGame", "true") == "true"; }, [](const Component& c) { return ComponentField::BoolStr(static_cast<const BillboardComponent&>(c).hiddenInGame); }),
    }
});
}
