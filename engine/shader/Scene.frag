#version 330 core

in vec3 vWorldNormal;
in vec4 vWorldTangent;
in vec3 vWorldPosition;
in float vViewDepth;
in vec2 vUV;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out uint EntityID;

uniform sampler2D u_BaseColorTexture;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicRoughnessTexture;
uniform sampler2D u_OcclusionTexture;
uniform sampler2D u_EmissiveTexture;
uniform sampler2D u_ShadowMap0;
uniform sampler2D u_ShadowMap1;
uniform sampler2D u_ShadowMap2;
uniform sampler2D u_ShadowMap3;

layout(std140) uniform FrameBlock
{
    mat4 u_View;
    mat4 u_Proj;
    mat4 u_ViewProjection;
    vec4 u_CameraPos;
    vec4 u_Viewport;
};

struct PointLight
{
    vec4 colorIntensity;
    vec4 positionRange;
};

layout(std140) uniform LightBlock
{
    vec4 u_DirColorIntensity;
    vec4 u_DirDirection;
    PointLight u_PointLights[16];
    ivec4 u_LightCounts;
};

layout(std140) uniform ShadowBlock
{
    mat4 u_ShadowMatrices[4];
    vec4 u_CascadeSplits;
    vec4 u_ShadowParams;
};

layout(std140) uniform DrawBlock
{
    mat4 u_Model;
    vec4 u_TintColor;
    uvec4 u_DrawIds;
};

layout(std140) uniform MaterialBlock
{
    vec4 u_BaseColorFactor;
    vec4 u_Surface;
    vec4 u_Emissive;
    ivec4 u_MaterialFlags;
};

float depthAt(int cascade, vec2 uv)
{
    if (cascade == 0) return texture(u_ShadowMap0, uv).r;
    if (cascade == 1) return texture(u_ShadowMap1, uv).r;
    if (cascade == 2) return texture(u_ShadowMap2, uv).r;
    return texture(u_ShadowMap3, uv).r;
}

vec2 shadowTexelSize(int cascade)
{
    if (cascade == 0) return 1.0 / vec2(textureSize(u_ShadowMap0, 0));
    if (cascade == 1) return 1.0 / vec2(textureSize(u_ShadowMap1, 0));
    if (cascade == 2) return 1.0 / vec2(textureSize(u_ShadowMap2, 0));
    return 1.0 / vec2(textureSize(u_ShadowMap3, 0));
}

float shadowVisibility(vec3 normal, vec3 lightDirection)
{
    int cascadeCount = clamp(int(u_ShadowParams.w + 0.5), 0, 4);
    if (cascadeCount == 0) return 1.0;
    int cascade = cascadeCount - 1;
    for (int i = 0; i < cascadeCount; ++i)
    {
        if (vViewDepth <= u_CascadeSplits[i])
        {
            cascade = i;
            break;
        }
    }
    vec4 lightClip = u_ShadowMatrices[cascade] * vec4(vWorldPosition, 1.0);
    if (lightClip.w <= 0.0) return 1.0;
    vec3 shadowCoord = lightClip.xyz / lightClip.w * 0.5 + 0.5;
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
        shadowCoord.z < 0.0 || shadowCoord.z > 1.0) return 1.0;
    float bias = u_ShadowParams.y + u_ShadowParams.z * (1.0 - max(dot(normal, lightDirection), 0.0));
    vec2 texel = shadowTexelSize(cascade);
    float visible = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            visible += shadowCoord.z - bias <= depthAt(cascade, shadowCoord.xy + vec2(x, y) * texel) ? 1.0 : 0.0;
    return mix(1.0, visible / 9.0, clamp(u_ShadowParams.x, 0.0, 1.0));
}

float distributionGGX(float nDotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = nDotH * nDotH * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * d * d, 0.0001);
}

float geometrySchlick(float nDotX, float roughness)
{
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return nDotX / max(nDotX * (1.0 - k) + k, 0.0001);
}

vec3 directLight(vec3 normal, vec3 viewDirection, vec3 lightDirection,
                 vec3 radiance, vec3 albedo, float metallic, float roughness)
{
    vec3 halfDirection = normalize(viewDirection + lightDirection);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    float nDotV = max(dot(normal, viewDirection), 0.0);
    if (nDotL <= 0.0 || nDotV <= 0.0) return vec3(0.0);
    float nDotH = max(dot(normal, halfDirection), 0.0);
    float hDotV = max(dot(halfDirection, viewDirection), 0.0);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - hDotV, 5.0);
    float distribution = distributionGGX(nDotH, roughness);
    float geometry = geometrySchlick(nDotV, roughness) * geometrySchlick(nDotL, roughness);
    vec3 specular = distribution * geometry * fresnel / max(4.0 * nDotV * nDotL, 0.0001);
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * albedo / 3.14159265;
    return (diffuse + specular) * radiance * nDotL;
}

void main()
{
    vec4 baseColor = texture(u_BaseColorTexture, vUV) * u_BaseColorFactor * u_TintColor;
    if (u_MaterialFlags.x == 1 && baseColor.a < u_Surface.w) discard;

    vec3 normal = normalize(vWorldNormal);
    if (u_MaterialFlags.z != 0)
    {
        vec3 tangent = normalize(vWorldTangent.xyz - normal * dot(normal, vWorldTangent.xyz));
        vec3 bitangent = normalize(cross(normal, tangent)) * vWorldTangent.w;
        normal = normalize(mat3(tangent, bitangent, normal) *
                           (texture(u_NormalMap, vUV).xyz * 2.0 - 1.0));
    }
    if (u_MaterialFlags.y != 0 && !gl_FrontFacing) normal = -normal;

    vec4 mr = texture(u_MetallicRoughnessTexture, vUV);
    float metallic = clamp(u_Surface.x * mr.b, 0.0, 1.0);
    float roughness = clamp(u_Surface.y * mr.g, 0.04, 1.0);
    float occlusion = mix(1.0, texture(u_OcclusionTexture, vUV).r, clamp(u_Surface.z, 0.0, 1.0));
    vec3 emissive = texture(u_EmissiveTexture, vUV).rgb * max(u_Emissive.rgb, vec3(0.0));
    vec3 viewDirection = normalize(u_CameraPos.xyz - vWorldPosition);
    // Keep unlit editor scenes readable until image-based ambient lighting lands.
    vec3 color = baseColor.rgb * 0.25 * occlusion;

    if (u_DirColorIntensity.w > 0.0)
    {
        vec3 lightDirection = normalize(-u_DirDirection.xyz);
        float visibility = shadowVisibility(normal, lightDirection);
        color += directLight(normal, viewDirection, lightDirection,
                             u_DirColorIntensity.rgb * u_DirColorIntensity.w * visibility,
                             baseColor.rgb, metallic, roughness);
    }
    for (int i = 0; i < min(u_LightCounts.x, 16); ++i)
    {
        vec3 offset = u_PointLights[i].positionRange.xyz - vWorldPosition;
        float distanceToLight = length(offset);
        float range = max(u_PointLights[i].positionRange.w, 0.001);
        float attenuation = pow(clamp(1.0 - distanceToLight / range, 0.0, 1.0), 2.0);
        color += directLight(normal, viewDirection, normalize(offset),
                             u_PointLights[i].colorIntensity.rgb *
                             u_PointLights[i].colorIntensity.w * attenuation,
                             baseColor.rgb, metallic, roughness);
    }

    FragColor = vec4(color + emissive, baseColor.a);
    EntityID = u_DrawIds.x;
}
