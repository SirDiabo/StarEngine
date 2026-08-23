#include "ShadingModel.h"
#include "Module.h"
#include "RenderSettings.h"
#include "ShadingTechniqueRegistry.h"
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

vec3 ComputeLighting(vec3 worldPos, vec3 worldNormal, float shadowFactor) {
    vec3 ambient = vec3(0.0);
    vec3 direct = vec3(0.0);
    for (int i = 0; i < lightCount; i++) {
        if (lights[i].type == LIGHT_GLOBAL) {
            ambient += lights[i].color * lights[i].intensity;
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
        direct += lights[i].color * lights[i].intensity * ndotl * atten;
    }
    return ambient + direct * shadowFactor;
}
)glsl";

const char* kShadowGLSL = R"glsl(
uniform int shadowMode;
uniform int shadowAAMethod;
uniform float shadowTexelSize;
uniform float shadowStrength;

uniform sampler2D shadowMap;
uniform mat4 shadowLightViewProj;
uniform vec3 shadowLightDir;

uniform vec3 shadowLightPos;
uniform sampler2D shadowCubeMap0;
uniform sampler2D shadowCubeMap1;
uniform sampler2D shadowCubeMap2;
uniform sampler2D shadowCubeMap3;
uniform sampler2D shadowCubeMap4;
uniform sampler2D shadowCubeMap5;
uniform mat4 shadowCubeViewProj0;
uniform mat4 shadowCubeViewProj1;
uniform mat4 shadowCubeViewProj2;
uniform mat4 shadowCubeViewProj3;
uniform mat4 shadowCubeViewProj4;
uniform mat4 shadowCubeViewProj5;

float SampleShadowMap(sampler2D map, mat4 lightViewProj, vec3 worldPos, vec3 worldNormal, vec3 lightDir) {
    vec4 lsPos = lightViewProj * vec4(worldPos, 1.0);
    vec3 proj = lsPos.xyz / lsPos.w;
    proj = proj * 0.5 + 0.5;

    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0) return 1.0;

    float ndotl = max(dot(worldNormal, lightDir), 0.0);
    float bias = max(0.0035 * (1.0 - ndotl), 0.0008);

    if (shadowAAMethod == 0) {
        float mapDepth = texture(map, proj.xy).r;
        return (proj.z - bias > mapDepth) ? (1.0 - shadowStrength) : 1.0;
    }

    if (shadowAAMethod == 2 || shadowAAMethod == 3) {
        float occluded = 0.0;
        for (int ix = -1; ix <= 2; ix++) {
            for (int iy = -1; iy <= 2; iy++) {
                vec2 offset = vec2(float(ix) - 0.5, float(iy) - 0.5) * shadowTexelSize * 1.5;
                float mapDepth = texture(map, proj.xy + offset).r;
                occluded += (proj.z - bias > mapDepth) ? (1.0 / 16.0) : 0.0;
            }
        }
        if (shadowAAMethod == 3) {
            return (occluded > 0.5) ? (1.0 - shadowStrength) : 1.0;
        }
        return 1.0 - occluded * shadowStrength;
    }

    vec2 poissonDisk[4] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760)
    );

    float occluded = 0.0;
    for (int i = 0; i < 4; i++) {
        vec2 offset = poissonDisk[i] * shadowTexelSize * 1.5;
        float mapDepth = texture(map, proj.xy + offset).r;
        occluded += (proj.z - bias > mapDepth) ? 0.25 : 0.0;
    }

    return 1.0 - occluded * shadowStrength;
}

float ComputeShadowFactor(vec3 worldPos, vec3 worldNormal) {
    if (shadowMode == 0) return 1.0;

    if (shadowMode == 1) {
        return SampleShadowMap(shadowMap, shadowLightViewProj, worldPos, worldNormal, shadowLightDir);
    }

    vec3 toFrag = worldPos - shadowLightPos;
    vec3 lightDir = normalize(-toFrag);
    vec3 a = abs(toFrag);

    if (a.x >= a.y && a.x >= a.z) {
        return toFrag.x > 0.0
            ? SampleShadowMap(shadowCubeMap0, shadowCubeViewProj0, worldPos, worldNormal, lightDir)
            : SampleShadowMap(shadowCubeMap1, shadowCubeViewProj1, worldPos, worldNormal, lightDir);
    }
    if (a.y >= a.x && a.y >= a.z) {
        return toFrag.y > 0.0
            ? SampleShadowMap(shadowCubeMap2, shadowCubeViewProj2, worldPos, worldNormal, lightDir)
            : SampleShadowMap(shadowCubeMap3, shadowCubeViewProj3, worldPos, worldNormal, lightDir);
    }
    return toFrag.z > 0.0
        ? SampleShadowMap(shadowCubeMap4, shadowCubeViewProj4, worldPos, worldNormal, lightDir)
        : SampleShadowMap(shadowCubeMap5, shadowCubeViewProj5, worldPos, worldNormal, lightDir);
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
    if (texelColor.a < 0.5) discard;
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

    fragLight = ComputeLighting(worldPos, worldNormal, 1.0);
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
        + kSampleAndOutput + kLightingGLSL + kShadowGLSL + R"glsl(
void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);
    if (texelColor.a < 0.5) discard;
    vec3 normal = normalize(fragWorldNormal);
    float shadowFactor = ComputeShadowFactor(fragWorldPos, normal);
    vec3 lightAccum = ComputeLighting(fragWorldPos, normal, shadowFactor);
    finalColor = texelColor * colDiffuse * fragColor * vec4(lightAccum, 1.0);
}
)glsl";
}

const char* kVertexUnlit = R"glsl(
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

const char* kFragmentCutout = R"glsl(
void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);
    if (texelColor.a < 0.5) discard;
    finalColor = texelColor * colDiffuse * fragColor;
}
)glsl";

struct LightLocs {
    int type = -1, position = -1, direction = -1, color = -1;
    int intensity = -1, radius = -1, innerCos = -1, outerCos = -1;
};

struct LitVariant {
    Shader shader = {0};
    bool loaded = false;
    int lightCountLoc = -1;
    LightLocs lightLocs[kMaxLights];

    int shadowModeLoc = -1;
    int shadowAAMethodLoc = -1;
    int shadowTexelSizeLoc = -1;
    int shadowStrengthLoc = -1;

    int shadowMapLoc = -1;
    int shadowLightViewProjLoc = -1;
    int shadowLightDirLoc = -1;

    int shadowLightPosLoc = -1;
    int shadowCubeMapLoc[6] = {-1, -1, -1, -1, -1, -1};
    int shadowCubeViewProjLoc[6] = {-1, -1, -1, -1, -1, -1};

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
        shadowModeLoc = GetShaderLocation(shader, "shadowMode");
        shadowAAMethodLoc = GetShaderLocation(shader, "shadowAAMethod");
        shadowTexelSizeLoc = GetShaderLocation(shader, "shadowTexelSize");
        shadowStrengthLoc = GetShaderLocation(shader, "shadowStrength");
        shadowMapLoc = GetShaderLocation(shader, "shadowMap");
        shadowLightViewProjLoc = GetShaderLocation(shader, "shadowLightViewProj");
        shadowLightDirLoc = GetShaderLocation(shader, "shadowLightDir");
        shadowLightPosLoc = GetShaderLocation(shader, "shadowLightPos");
        for (int i = 0; i < 6; i++) {
            shadowCubeMapLoc[i] = GetShaderLocation(shader, TextFormat("shadowCubeMap%d", i));
            shadowCubeViewProjLoc[i] = GetShaderLocation(shader, TextFormat("shadowCubeViewProj%d", i));
        }
        loaded = true;
    }
};

LitVariant vertexVariant;
LitVariant pixelVariant;

struct UnlitVariant {
    Shader shader = {0};
    bool loaded = false;

    void EnsureLoaded() {
        if (loaded) return;
        std::string vs = std::string(kAttribsAndVaryings) + kVertexUnlit;
        std::string fs = std::string("#version 330\nin vec2 fragTexCoord;\nin vec4 fragColor;\n") + kSampleAndOutput + kFragmentCutout;
        shader = LoadShaderFromMemory(vs.c_str(), fs.c_str());
        loaded = true;
    }
};

UnlitVariant unlitVariant;

int shadowMode = 0; // 0 = none, 1 = single map, 2 = six-face cube

unsigned int shadowSourceTextureId = 0;
Matrix shadowSourceLightViewProj = MatrixIdentity();
Vector3 shadowSourceLightDir = {0.0f, -1.0f, 0.0f};

Vector3 shadowSourceLightPos = {0.0f, 0.0f, 0.0f};
unsigned int shadowSourceCubeTextureIds[6] = {0, 0, 0, 0, 0, 0};
Matrix shadowSourceCubeViewProj[6] = {MatrixIdentity(), MatrixIdentity(), MatrixIdentity(), MatrixIdentity(), MatrixIdentity(), MatrixIdentity()};

float shadowSourceTexelSize = 0.0f;
float shadowSourceStrength = 0.0f;
int shadowSourceAAMethod = 1;

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
    const std::string& shadingId = RenderSettings::Get().GetShadingType();
    if (shadingId.empty() || shadingId == "Unlit") return false;
    return ShadingTechniqueRegistry::Get().FindAvailable(shadingId) != nullptr;
}

Shader& ShadingModel::GetLitShader() {
    return ActiveVariant().shader;
}

Shader ShadingModel::GetDefaultShader() {
    unlitVariant.EnsureLoaded();
    return unlitVariant.shader;
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

    SetShaderValue(variant.shader, variant.shadowModeLoc, &shadowMode, SHADER_UNIFORM_INT);
    SetShaderValue(variant.shader, variant.shadowAAMethodLoc, &shadowSourceAAMethod, SHADER_UNIFORM_INT);
    if (shadowMode == 1) {
        Texture2D shadowTex = {0};
        shadowTex.id = shadowSourceTextureId;
        SetShaderValueTexture(variant.shader, variant.shadowMapLoc, shadowTex);
        SetShaderValueMatrix(variant.shader, variant.shadowLightViewProjLoc, shadowSourceLightViewProj);
        float dir[3] = {shadowSourceLightDir.x, shadowSourceLightDir.y, shadowSourceLightDir.z};
        SetShaderValue(variant.shader, variant.shadowLightDirLoc, dir, SHADER_UNIFORM_VEC3);
        SetShaderValue(variant.shader, variant.shadowTexelSizeLoc, &shadowSourceTexelSize, SHADER_UNIFORM_FLOAT);
        SetShaderValue(variant.shader, variant.shadowStrengthLoc, &shadowSourceStrength, SHADER_UNIFORM_FLOAT);
    } else if (shadowMode == 2) {
        float pos[3] = {shadowSourceLightPos.x, shadowSourceLightPos.y, shadowSourceLightPos.z};
        SetShaderValue(variant.shader, variant.shadowLightPosLoc, pos, SHADER_UNIFORM_VEC3);
        for (int i = 0; i < 6; i++) {
            Texture2D faceTex = {0};
            faceTex.id = shadowSourceCubeTextureIds[i];
            SetShaderValueTexture(variant.shader, variant.shadowCubeMapLoc[i], faceTex);
            SetShaderValueMatrix(variant.shader, variant.shadowCubeViewProjLoc[i], shadowSourceCubeViewProj[i]);
        }
        SetShaderValue(variant.shader, variant.shadowTexelSizeLoc, &shadowSourceTexelSize, SHADER_UNIFORM_FLOAT);
        SetShaderValue(variant.shader, variant.shadowStrengthLoc, &shadowSourceStrength, SHADER_UNIFORM_FLOAT);
    }
}

void ShadingModel::ResetShadowSource() {
    shadowMode = 0;
}

void ShadingModel::SetShadowSource(unsigned int shadowDepthTextureId, Matrix lightViewProj, Vector3 lightDirToSurface, int shadowMapSize, float strength, int aaMethod) {
    shadowMode = 1;
    shadowSourceTextureId = shadowDepthTextureId;
    shadowSourceLightViewProj = lightViewProj;
    shadowSourceLightDir = lightDirToSurface;
    shadowSourceTexelSize = shadowMapSize > 0 ? 1.0f / (float)shadowMapSize : 0.0f;
    shadowSourceStrength = strength;
    shadowSourceAAMethod = aaMethod;
}

void ShadingModel::SetShadowSourceCube(Vector3 lightPos, const unsigned int faceTextureIds[6], const Matrix faceViewProj[6], int shadowMapSize, float strength, int aaMethod) {
    shadowMode = 2;
    shadowSourceLightPos = lightPos;
    for (int i = 0; i < 6; i++) {
        shadowSourceCubeTextureIds[i] = faceTextureIds[i];
        shadowSourceCubeViewProj[i] = faceViewProj[i];
    }
    shadowSourceTexelSize = shadowMapSize > 0 ? 1.0f / (float)shadowMapSize : 0.0f;
    shadowSourceStrength = strength;
    shadowSourceAAMethod = aaMethod;
}