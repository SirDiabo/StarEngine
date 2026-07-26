#include "ShadingModel.h"
#include "Core/Module.h"
#include "Core/RenderSettings.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/LightComponent.h"
#include "rlgl.h"
#include "raymath.h"
#include <string>

namespace {

const int kMaxLights = 4;

const char* kLightingGLSL = R"glsl(
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

vec3 ComputeLighting(vec3 worldPos, vec3 worldNormal) {
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
    return accum;
}
)glsl";

const char* kAttribsAndVaryings = R"glsl(
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
)glsl";

const char* kSampleAndOutput = R"glsl(
uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;
)glsl";

// PerVertex
const char* kFragmentPassthrough = R"glsl(
in vec3 fragLight;

void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);
    finalColor = texelColor * colDiffuse * fragColor * vec4(fragLight, 1.0);
}
)glsl";

// PerPixel
const char* kVertexPassthrough = R"glsl(
out vec3 fragWorldPos;
out vec3 fragWorldNormal;

void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragWorldPos = vec3(matModel * vec4(vertexPosition, 1.0));
    fragWorldNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

std::string BuildPerVertexVS() {
    return std::string(kAttribsAndVaryings) + "out vec3 fragLight;\n" + kLightingGLSL + R"glsl(
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    vec3 worldPos = vec3(matModel * vec4(vertexPosition, 1.0));
    vec3 worldNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));

    fragLight = ComputeLighting(worldPos, worldNormal);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";
}

std::string BuildPerVertexFS() {
    return std::string("#version 330\nin vec2 fragTexCoord;\nin vec4 fragColor;\n") + kSampleAndOutput + kFragmentPassthrough;
}

std::string BuildPerPixelVS() {
    return std::string(kAttribsAndVaryings) + kVertexPassthrough;
}

std::string BuildPerPixelFS() {
    return std::string("#version 330\nin vec2 fragTexCoord;\nin vec4 fragColor;\nin vec3 fragWorldPos;\nin vec3 fragWorldNormal;\n")
        + kSampleAndOutput + kLightingGLSL + R"glsl(
void main() {
    vec3 lightAccum = ComputeLighting(fragWorldPos, normalize(fragWorldNormal));
    vec4 texelColor = texture(texture0, fragTexCoord);
    finalColor = texelColor * colDiffuse * fragColor * vec4(lightAccum, 1.0);
}
)glsl";
}

struct LightLocs {
    int type = -1, position = -1, direction = -1, color = -1;
    int intensity = -1, radius = -1, innerCos = -1, outerCos = -1;
};

struct LitVariant {
    Shader shader = {0};
    bool loaded = false;
    int lightCountLoc = -1;
    LightLocs lightLocs[kMaxLights];

    void EnsureLoaded(const std::string& vs, const std::string& fs) {
        if (loaded) return;
        shader = LoadShaderFromMemory(vs.c_str(), fs.c_str());
        lightCountLoc = GetShaderLocation(shader, "lightCount");
        for (int i = 0; i < kMaxLights; i++) {
            lightLocs[i].type      = GetShaderLocation(shader, TextFormat("lights[%d].type", i));
            lightLocs[i].position  = GetShaderLocation(shader, TextFormat("lights[%d].position", i));
            lightLocs[i].direction = GetShaderLocation(shader, TextFormat("lights[%d].direction", i));
            lightLocs[i].color     = GetShaderLocation(shader, TextFormat("lights[%d].color", i));
            lightLocs[i].intensity = GetShaderLocation(shader, TextFormat("lights[%d].intensity", i));
            lightLocs[i].radius    = GetShaderLocation(shader, TextFormat("lights[%d].radius", i));
            lightLocs[i].innerCos  = GetShaderLocation(shader, TextFormat("lights[%d].innerCos", i));
            lightLocs[i].outerCos  = GetShaderLocation(shader, TextFormat("lights[%d].outerCos", i));
        }
        loaded = true;
    }
};

LitVariant vertexVariant;
LitVariant pixelVariant;

LitVariant& ActiveVariant() {
    if (RenderSettings::Get().GetLightingQuality() == LightingQuality::PerPixel) {
        pixelVariant.EnsureLoaded(BuildPerPixelVS(), BuildPerPixelFS());
        return pixelVariant;
    }
    vertexVariant.EnsureLoaded(BuildPerVertexVS(), BuildPerVertexFS());
    return vertexVariant;
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
    return ActiveVariant().shader;
}

Shader ShadingModel::GetDefaultShader() {
    Shader shader = {0};
    shader.id = rlGetShaderIdDefault();
    shader.locs = rlGetShaderLocsDefault();
    return shader;
}

const char* ShadingModel::GetLightingGLSL() {
    return kLightingGLSL;
}

void ShadingModel::UpdateLightUniforms() {
    if (!ShouldUseLitShading()) return;
    LitVariant& variant = ActiveVariant();

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

            const LightLocs& locs = variant.lightLocs[count];
            SetShaderValue(variant.shader, locs.type, &type, SHADER_UNIFORM_INT);
            SetShaderValue(variant.shader, locs.position, pos, SHADER_UNIFORM_VEC3);
            SetShaderValue(variant.shader, locs.direction, dirArr, SHADER_UNIFORM_VEC3);
            SetShaderValue(variant.shader, locs.color, color, SHADER_UNIFORM_VEC3);
            SetShaderValue(variant.shader, locs.intensity, &lc->intensity, SHADER_UNIFORM_FLOAT);
            SetShaderValue(variant.shader, locs.radius, &lc->radius, SHADER_UNIFORM_FLOAT);
            SetShaderValue(variant.shader, locs.innerCos, &innerCos, SHADER_UNIFORM_FLOAT);
            SetShaderValue(variant.shader, locs.outerCos, &outerCos, SHADER_UNIFORM_FLOAT);

            count++;
        }
    }

    SetShaderValue(variant.shader, variant.lightCountLoc, &count, SHADER_UNIFORM_INT);
}