#version 330 core
struct DirLight {
    vec3 color;
    float intensity;
    vec3 direction;
    float pad0;
};

struct PointLight {
    vec3 color;
    float intensity;
    vec3 position;
    float range;
};

const int MAX_POINT_LIGHTS = 16;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 vTangent;

layout(location=0) out vec4 FragColor;
layout(location=1) out uint EntityID;

uniform uint u_EntityID;
uniform vec3 u_CameraPos;
uniform DirLight u_DirLight;
uniform PointLight u_PointLights[MAX_POINT_LIGHTS];
uniform int u_PointCount;

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
uniform int u_HasNormalMap;

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
    vec3 albedo = (u_AlbedoColor * u_TintColor).rgb * texture(u_AlbedoMap, vUV).rgb;
    vec2 mrTex = texture(u_MRMap, vUV).rg;
    float metallic  = clamp(u_Metallic * mrTex.r, 0.0, 1.0);
    float roughness = clamp(u_Roughness * mrTex.g, 0.04, 1.0);
    float ao        = clamp(u_AO * texture(u_AOMap, vUV).r, 0.0, 1.0);
    vec3 emissiveColor = texture(u_EmissiveMap, vUV).rgb * max(0.0, u_Emissive);

    vec3 N = getNormal();
    vec3 V = normalize(u_CameraPos - vWorldPos);

    vec3 ambient = 0.05 * albedo * ao;
    vec3 color = ambient;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 ks = F0;
    vec3 kd = (vec3(1.0) - ks) * (1.0 - metallic);
    float specPow = mix(8.0, 128.0, 1.0 - roughness);

    vec3 Ld = normalize(-u_DirLight.direction);
    float ndl = max(dot(N, Ld), 0.0);
    vec3 H = normalize(Ld + V);
    float spec = pow(max(dot(N, H), 0.0), specPow);
    color += (kd * ndl * albedo + ks * spec) * u_DirLight.color * u_DirLight.intensity;

    for (int i = 0; i < u_PointCount && i < MAX_POINT_LIGHTS; ++i) {
        vec3 Lp = u_PointLights[i].position - vWorldPos;
        float dist = length(Lp);
        if (dist > u_PointLights[i].range) continue;

        float invDist = 1.0 / max(dist, 1e-4);
        Lp *= invDist;

        float att = 1.0 - clamp(dist / u_PointLights[i].range, 0.0, 1.0);
        float ndl_p = max(dot(N, Lp), 0.0);

        vec3 Hp = normalize(Lp + V);
        float specp = pow(max(dot(N, Hp), 0.0), specPow);

        color += att * ((kd * ndl_p * albedo + ks * specp) *
                        u_PointLights[i].color * u_PointLights[i].intensity);
    }

    color += emissiveColor;
    FragColor = vec4(color, u_AlbedoColor.a * u_TintColor.a);
    EntityID  = u_EntityID;
}
