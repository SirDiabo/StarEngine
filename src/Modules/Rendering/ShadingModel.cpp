#include "ShadingModel.h"
#include "Core/Module.h"
#include "Core/RenderSettings.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/LightComponent.h"
#include "rlgl.h"
#include "raymath.h"

namespace {

const int kMaxLights = 4;

const char* kLitVertexShader = R"glsl(
#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragLight;

#define MAX_LIGHTS 4
#define LIGHT_POINT 0
#define LIGHT_RECTANGLE 1
#define LIGHT_SPOT 2
#define LIGHT_GLOBAL 3
#define LIGHT_SUN 4

struct Light {
    int type;
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float radius;
    float innerCos;
    float outerCos;
};

uniform Light lights[MAX_LIGHTS];
uniform int lightCount;

void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    vec3 worldPos = vec3(matModel * vec4(vertexPosition, 1.0));
    vec3 worldNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));

    vec3 accum = vec3(0.0);
    for (int i = 0; i < lightCount; i++) {
        if (lights[i].type == LIGHT_GLOBAL) {
            accum += lights[i].color * lights[i].intensity;
            continue;
        }

        vec3 toLight;
        float atten = 1.0;

        if (lights[i].type == LIGHT_SUN) {
            toLight = normalize(-lights[i].direction);
        } else {
            vec3 delta = lights[i].position - worldPos;
            float dist = length(delta);
            toLight = delta / max(dist, 0.0001);

            atten = clamp(1.0 - dist / max(lights[i].radius, 0.0001), 0.0, 1.0);
            atten *= atten;

            if (lights[i].type == LIGHT_SPOT) {
                float cosAngle = dot(-toLight, normalize(lights[i].direction));
                float coneRange = max(lights[i].innerCos - lights[i].outerCos, 0.0001);
                atten *= clamp((cosAngle - lights[i].outerCos) / coneRange, 0.0, 1.0);
            }
        }

        float ndotl = max(dot(worldNormal, toLight), 0.0);
        accum += lights[i].color * lights[i].intensity * ndotl * atten;
    }

    fragLight = accum;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

const char* kLitFragmentShader = R"glsl(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragLight;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);
    finalColor = texelColor * colDiffuse * fragColor * vec4(fragLight, 1.0);
}
)glsl";

struct LightLocs {
    int type = -1, position = -1, direction = -1, color = -1;
    int intensity = -1, radius = -1, innerCos = -1, outerCos = -1;
};

Shader litShader = {0};
bool litShaderLoaded = false;
int lightCountLoc = -1;
LightLocs lightLocs[kMaxLights];

void EnsureLitShaderLoaded() {
    if (litShaderLoaded) return;

    litShader = LoadShaderFromMemory(kLitVertexShader, kLitFragmentShader);
    lightCountLoc = GetShaderLocation(litShader, "lightCount");
    for (int i = 0; i < kMaxLights; i++) {
        lightLocs[i].type      = GetShaderLocation(litShader, TextFormat("lights[%d].type", i));
        lightLocs[i].position  = GetShaderLocation(litShader, TextFormat("lights[%d].position", i));
        lightLocs[i].direction = GetShaderLocation(litShader, TextFormat("lights[%d].direction", i));
        lightLocs[i].color     = GetShaderLocation(litShader, TextFormat("lights[%d].color", i));
        lightLocs[i].intensity = GetShaderLocation(litShader, TextFormat("lights[%d].intensity", i));
        lightLocs[i].radius    = GetShaderLocation(litShader, TextFormat("lights[%d].radius", i));
        lightLocs[i].innerCos  = GetShaderLocation(litShader, TextFormat("lights[%d].innerCos", i));
        lightLocs[i].outerCos  = GetShaderLocation(litShader, TextFormat("lights[%d].outerCos", i));
    }
    litShaderLoaded = true;
}

Vector3 DirectionFromRotation(Vector3 rot) {
    float yr = rot.y * DEG2RAD;
    float pr = rot.x * DEG2RAD;
    return {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
}

} // namespace

bool ShadingModel::ShouldUseLitShading() {
    return ModuleManager::Get().IsModuleEnabled("Rendering")
        && RenderSettings::Get().GetShadingType() != ShadingType::Unlit;
}

Shader& ShadingModel::GetLitShader() {
    EnsureLitShaderLoaded();
    return litShader;
}

Shader ShadingModel::GetDefaultShader() {
    Shader shader = {0};
    shader.id = rlGetShaderIdDefault();
    shader.locs = rlGetShaderLocsDefault();
    return shader;
}

void ShadingModel::UpdateLightUniforms() {
    if (!ShouldUseLitShading()) return;
    EnsureLitShaderLoaded();

    int count = 0;
    for (const auto& actor : World::Get().GetActors()) {
        for (Component* comp : actor->GetComponents()) {
            LightComponent* lc = dynamic_cast<LightComponent*>(comp);
            if (!lc || count >= kMaxLights) continue;

            Vector3 worldPos = actor->GetComponentWorldPosition(lc);
            Vector3 worldRot = actor->GetComponentWorldRotation(lc);
            Vector3 dir = DirectionFromRotation(worldRot);

            int type = (int)lc->lightType;
            float pos[3] = {worldPos.x, worldPos.y, worldPos.z};
            float dirArr[3] = {dir.x, dir.y, dir.z};
            float color[3] = {lc->lightColor.x, lc->lightColor.y, lc->lightColor.z};
            float innerCos = cosf(lc->innerConeAngle * DEG2RAD);
            float outerCos = cosf(lc->outerConeAngle * DEG2RAD);

            const LightLocs& locs = lightLocs[count];
            SetShaderValue(litShader, locs.type, &type, SHADER_UNIFORM_INT);
            SetShaderValue(litShader, locs.position, pos, SHADER_UNIFORM_VEC3);
            SetShaderValue(litShader, locs.direction, dirArr, SHADER_UNIFORM_VEC3);
            SetShaderValue(litShader, locs.color, color, SHADER_UNIFORM_VEC3);
            SetShaderValue(litShader, locs.intensity, &lc->intensity, SHADER_UNIFORM_FLOAT);
            SetShaderValue(litShader, locs.radius, &lc->radius, SHADER_UNIFORM_FLOAT);
            SetShaderValue(litShader, locs.innerCos, &innerCos, SHADER_UNIFORM_FLOAT);
            SetShaderValue(litShader, locs.outerCos, &outerCos, SHADER_UNIFORM_FLOAT);

            count++;
        }
    }

    SetShaderValue(litShader, lightCountLoc, &count, SHADER_UNIFORM_INT);
}
