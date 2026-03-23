#version 330 core
const int MAX_POINT_LIGHTS = 16;
const int MAX_SHADOW_CASCADES = 4;

struct PointLightData
{
    vec4 colorIntensity;
    vec4 positionRange;
};

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 vTangent;

layout(location=0) out vec4 FragColor;
layout(location=1) out uint EntityID;

uniform uint u_EntityID;

layout(std140) uniform FrameBlock
{
    mat4 u_View;
    mat4 u_Proj;
    mat4 u_ViewProjection;
    vec4 u_CameraPos;
    vec4 u_Viewport;
};

layout(std140) uniform LightBlock
{
    vec4 u_DirLightColorIntensity;
    vec4 u_DirLightDirection;
    PointLightData u_PointLights[MAX_POINT_LIGHTS];
    ivec4 u_LightCounts;
};

uniform vec4 u_AlbedoColor;
uniform vec4 u_TintColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform float u_Emissive;

uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MRMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissiveMap;
uniform sampler2D u_ShadowMaps[MAX_SHADOW_CASCADES];
uniform int u_HasNormalMap;
uniform int u_SurfaceMode;
uniform float u_AlphaCutoff;
uniform int u_ShadowsEnabled;
uniform int u_ShadowCascadeCount;
uniform mat4 u_LightViewProjections[MAX_SHADOW_CASCADES];
uniform float u_ShadowCascadeSplits[MAX_SHADOW_CASCADES];
uniform float u_ShadowStrength;
uniform float u_ShadowBiasConst;
uniform float u_ShadowBiasSlope;

int selectShadowCascade(float viewDepth)
{
    if (u_ShadowCascadeCount <= 0)
        return -1;

    for (int i = 0; i < u_ShadowCascadeCount && i < MAX_SHADOW_CASCADES; ++i)
    {
        if (viewDepth <= u_ShadowCascadeSplits[i])
            return i;
    }

    return u_ShadowCascadeCount - 1;
}

float sampleShadow(int cascadeIndex, vec3 normal, vec3 lightDir)
{
    if (u_ShadowsEnabled == 0 || cascadeIndex < 0 || cascadeIndex >= u_ShadowCascadeCount)
        return 0.0;

    vec4 lightSpacePos = u_LightViewProjections[cascadeIndex] * vec4(vWorldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / max(lightSpacePos.w, 1e-6);
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float bias = max(u_ShadowBiasSlope * (1.0 - dot(normal, lightDir)), u_ShadowBiasConst);
    vec2 texelSize = 1.0 / vec2(textureSize(u_ShadowMaps[cascadeIndex], 0));
    float shadow = 0.0;

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(u_ShadowMaps[cascadeIndex], projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}

vec3 getNormal() {
    vec3 N = normalize(vNormal);
    if (u_HasNormalMap == 0)
        return N;

    if (length(vTangent.xyz) < 1e-4)
        return N;
    vec3 T = normalize(vTangent.xyz);
    T = normalize(T - N * dot(N, T));
    vec3 B = normalize(cross(N, T)) * vTangent.w;

    vec3 nMap = texture(u_NormalMap, vUV).xyz * 2.0 - 1.0;
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * nMap);
}

void main() {
    vec4 albedoSample = texture(u_AlbedoMap, vUV);
    vec4 albedoTint = u_AlbedoColor * u_TintColor;
    vec3 albedo = albedoTint.rgb * albedoSample.rgb;
    float alpha = albedoTint.a * albedoSample.a;

    if (u_SurfaceMode == 1 && alpha < u_AlphaCutoff)
        discard;

    vec2 mrTex = texture(u_MRMap, vUV).rg;
    float metallic  = clamp(u_Metallic * mrTex.r, 0.0, 1.0);
    float roughness = clamp(u_Roughness * mrTex.g, 0.04, 1.0);
    float ao        = clamp(u_AO * texture(u_AOMap, vUV).r, 0.0, 1.0);
    vec3 emissiveColor = texture(u_EmissiveMap, vUV).rgb * max(0.0, u_Emissive);

    vec3 N = getNormal();
    vec3 V = normalize(u_CameraPos.xyz - vWorldPos);

    vec3 ambient = 0.05 * albedo * ao;
    vec3 color = ambient;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 ks = F0;
    vec3 kd = (vec3(1.0) - ks) * (1.0 - metallic);
    float specPow = mix(8.0, 128.0, 1.0 - roughness);

    vec3 Ld = normalize(-u_DirLightDirection.xyz);
    float ndl = max(dot(N, Ld), 0.0);
    vec3 H = normalize(Ld + V);
    float spec = pow(max(dot(N, H), 0.0), specPow);
    float viewDepth = -(u_View * vec4(vWorldPos, 1.0)).z;
    int shadowCascadeIndex = selectShadowCascade(viewDepth);
    float shadow = sampleShadow(shadowCascadeIndex, N, Ld);
    color += (1.0 - shadow * u_ShadowStrength) * (kd * ndl * albedo + ks * spec) * u_DirLightColorIntensity.rgb * u_DirLightColorIntensity.a;

    for (int i = 0; i < u_LightCounts.x && i < MAX_POINT_LIGHTS; ++i) {
        vec3 lightColor = u_PointLights[i].colorIntensity.rgb;
        float lightIntensity = u_PointLights[i].colorIntensity.a;
        vec3 lightPosition = u_PointLights[i].positionRange.xyz;
        float lightRange = u_PointLights[i].positionRange.a;

        vec3 Lp = lightPosition - vWorldPos;
        float dist = length(Lp);
        if (dist > lightRange) continue;

        float invDist = 1.0 / max(dist, 1e-4);
        Lp *= invDist;

        float att = 1.0 - clamp(dist / lightRange, 0.0, 1.0);
        float ndl_p = max(dot(N, Lp), 0.0);

        vec3 Hp = normalize(Lp + V);
        float specp = pow(max(dot(N, Hp), 0.0), specPow);

        color += att * ((kd * ndl_p * albedo + ks * specp) *
                        lightColor * lightIntensity);
    }

    color += emissiveColor;
    FragColor = vec4(color, alpha);
    EntityID  = u_EntityID;
}
