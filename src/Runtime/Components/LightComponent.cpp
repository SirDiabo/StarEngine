#include "LightComponent.h"
#include "Modules/Stargraph/ComponentRegistry.h"
#include "Core/SafeParse.h"

void LightComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddVector3("Light Color", &lightColor.x);
    outProperties.AddFloat("Intensity", &intensity);
    outProperties.AddFloat("Radius", &radius);
    outProperties.AddFloat("Inner Cone Angle", &innerConeAngle);
    outProperties.AddFloat("Outer Cone Angle", &outerConeAngle);
    outProperties.AddFloat("Width", &width);
    outProperties.AddFloat("Height", &height);
}

void LightComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}

namespace {
bool NotGlobalOrSun(const ComponentDecl& d) {
    std::string t = d.Get("LightType", "Point");
    return t != "Global" && t != "Sun";
}
bool IsSpot(const ComponentDecl& d) { return d.Get("LightType", "Point") == "Spot"; }
bool IsRectangle(const ComponentDecl& d) { return d.Get("LightType", "Point") == "Rectangle"; }

ComponentRegistrar registrar_LightComponent({
    "LightComponent",
    []() -> Component* { return new LightComponent(); },
    {
        ComponentField::Enm("LightType", "Light Type", {"Point", "Rectangle", "Spot", "Global", "Sun"}, "Point",
            [](Component& c, const ComponentDecl& d) {
                std::string t = d.Get("LightType", "Point");
                auto& lc = static_cast<LightComponent&>(c);
                lc.lightType = t == "Rectangle" ? LightComponent::Rectangle
                             : t == "Spot"      ? LightComponent::Spot
                             : t == "Global"    ? LightComponent::Global
                             : t == "Sun"       ? LightComponent::Sun
                                                 : LightComponent::Point;
            },
            [](const Component& c) -> std::string {
                switch (static_cast<const LightComponent&>(c).lightType) {
                    case LightComponent::Rectangle: return "Rectangle";
                    case LightComponent::Spot:      return "Spot";
                    case LightComponent::Global:    return "Global";
                    case LightComponent::Sun:       return "Sun";
                    default:                        return "Point";
                }
            }),
        ComponentField::Flt("LightColorR", "Color R", 1.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).lightColor.x = SafeParse::ToFloat(d.Get("LightColorR", "1")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const LightComponent&>(c).lightColor.x); }),
        ComponentField::Flt("LightColorG", "Color G", 1.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).lightColor.y = SafeParse::ToFloat(d.Get("LightColorG", "1")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const LightComponent&>(c).lightColor.y); }),
        ComponentField::Flt("LightColorB", "Color B", 1.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).lightColor.z = SafeParse::ToFloat(d.Get("LightColorB", "1")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const LightComponent&>(c).lightColor.z); }),
        ComponentField::Flt("LightIntensity", "Intensity", 1.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).intensity = SafeParse::ToFloat(d.Get("LightIntensity", "1")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const LightComponent&>(c).intensity); }),
        [] { ComponentFieldDesc f = ComponentField::Flt("LightRadius", "Radius", 500.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).radius = SafeParse::ToFloat(d.Get("LightRadius", "500")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const LightComponent&>(c).radius); });
             f.visibleIf = NotGlobalOrSun; return f; }(),
        [] { ComponentFieldDesc f = ComponentField::Flt("LightInnerConeAngle", "Inner Cone Angle", 25.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).innerConeAngle = SafeParse::ToFloat(d.Get("LightInnerConeAngle", "25")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const LightComponent&>(c).innerConeAngle); });
             f.visibleIf = IsSpot; return f; }(),
        [] { ComponentFieldDesc f = ComponentField::Flt("LightOuterConeAngle", "Outer Cone Angle", 35.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).outerConeAngle = SafeParse::ToFloat(d.Get("LightOuterConeAngle", "35")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const LightComponent&>(c).outerConeAngle); });
             f.visibleIf = IsSpot; return f; }(),
        [] { ComponentFieldDesc f = ComponentField::Flt("LightWidth", "Width", 100.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).width = SafeParse::ToFloat(d.Get("LightWidth", "100")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const LightComponent&>(c).width); });
             f.visibleIf = IsRectangle; return f; }(),
        [] { ComponentFieldDesc f = ComponentField::Flt("LightHeight", "Height", 100.0f,
                [](Component& c, const ComponentDecl& d) { static_cast<LightComponent&>(c).height = SafeParse::ToFloat(d.Get("LightHeight", "100")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const LightComponent&>(c).height); });
             f.visibleIf = IsRectangle; return f; }(),
    }
});
}
