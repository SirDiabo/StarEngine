#include "BasicActorDefs.h"

const std::vector<BasicActorDef>& GetBasicActorDefs() {
    static const std::vector<BasicActorDef> table = [] {
        std::vector<BasicActorDef> defs;

        BasicActorDef mesh;
        mesh.label = "Mesh";
        mesh.accentColor = {90, 150, 210, 255};
        mesh.iconFile = "icon_actor_mesh.png";
        mesh.kindName = "MeshActor";
        mesh.isMeshActor = true;
        defs.push_back(mesh);

        auto light = [](const char* label, Color color, const char* lightType, const char* iconFile, const char* kindName) {
            BasicActorDef def;
            def.label = label;
            def.accentColor = color;
            def.iconFile = iconFile;
            def.kindName = kindName;
            def.componentType = "LightComponent";
            def.defaultComponent.componentType = "LightComponent";
            def.defaultComponent.fields["LightType"] = lightType;
            return def;
        };

        defs.push_back(light("Point Light", {240, 210, 120, 255}, "Point", "icon_actor_light_point.png", "PointLightActor"));
        defs.push_back(light("Sun Light", {250, 190, 90, 255}, "Sun", "icon_actor_light_sun.png", "SunLightActor"));
        defs.push_back(light("Spot Light", {230, 170, 255, 255}, "Spot", "icon_actor_light_spot.png", "SpotLightActor"));
        defs.push_back(light("Rect Light", {150, 210, 255, 255}, "Rectangle", "icon_actor_light_rect.png", "RectLightActor"));
        defs.push_back(light("Global Light", {200, 200, 220, 255}, "Global", "icon_actor_light_global.png", "GlobalLightActor"));

        BasicActorDef camera;
        camera.label = "Camera";
        camera.accentColor = {120, 200, 140, 255};
        camera.iconFile = "icon_actor_camera.png";
        camera.kindName = "CameraActor";
        camera.componentType = "CameraComponent";
        camera.defaultComponent.componentType = "CameraComponent";
        defs.push_back(camera);

        BasicActorDef audio;
        audio.label = "Audio";
        audio.accentColor = {220, 130, 130, 255};
        audio.iconFile = "icon_actor_audio.png";
        audio.kindName = "AudioActor";
        audio.componentType = "AudioComponent";
        audio.defaultComponent.componentType = "AudioComponent";
        defs.push_back(audio);

        return defs;
    }();
    return table;
}
