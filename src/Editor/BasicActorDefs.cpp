#include "BasicActorDefs.h"

const std::vector<BasicActorDef>& GetBasicActorDefs() {
    static const std::vector<BasicActorDef> table = [] {
        std::vector<BasicActorDef> defs;

        BasicActorDef mesh;
        mesh.label = "Mesh";
        mesh.accentColor = {90, 150, 210, 255};
        mesh.isMeshActor = true;
        defs.push_back(mesh);

        auto light = [](const char* label, Color color, const char* lightType) {
            BasicActorDef def;
            def.label = label;
            def.accentColor = color;
            def.componentType = "LightComponent";
            def.defaultComponent.componentType = "LightComponent";
            def.defaultComponent.lightType = lightType;
            return def;
        };

        defs.push_back(light("Point Light", {240, 210, 120, 255}, "Point"));
        defs.push_back(light("Sun Light", {250, 190, 90, 255}, "Sun"));
        defs.push_back(light("Spot Light", {230, 170, 255, 255}, "Spot"));
        defs.push_back(light("Rect Light", {150, 210, 255, 255}, "Rectangle"));
        defs.push_back(light("Global Light", {200, 200, 220, 255}, "Global"));

        BasicActorDef camera;
        camera.label = "Camera";
        camera.accentColor = {120, 200, 140, 255};
        camera.componentType = "CameraComponent";
        camera.defaultComponent.componentType = "CameraComponent";
        defs.push_back(camera);

        BasicActorDef audio;
        audio.label = "Audio";
        audio.accentColor = {220, 130, 130, 255};
        audio.componentType = "AudioComponent";
        audio.defaultComponent.componentType = "AudioComponent";
        defs.push_back(audio);

        return defs;
    }();
    return table;
}
