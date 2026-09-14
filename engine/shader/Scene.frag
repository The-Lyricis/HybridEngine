#version 330 core

in vec3 vWorldNormal;
in vec4 vWorldTangent;
in vec2 vUV;

uniform sampler2D u_BaseColorTexture;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicRoughnessTexture;
uniform sampler2D u_OcclusionTexture;
uniform sampler2D u_EmissiveTexture;

layout(location=0) out vec4 FragColor;
layout(location=1) out uint EntityID;

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

void main()
{
    vec4 baseColor = texture(u_BaseColorTexture, vUV) * u_BaseColorFactor * u_TintColor;
    if (u_MaterialFlags.x == 1 && baseColor.a < u_Surface.w)
        discard;

    vec3 normal = normalize(vWorldNormal);
    if (u_MaterialFlags.z != 0)
    {
        vec3 tangent = normalize(vWorldTangent.xyz - normal * dot(normal, vWorldTangent.xyz));
        vec3 bitangent = normalize(cross(normal, tangent)) * vWorldTangent.w;
        normal = normalize(mat3(tangent, bitangent, normal) * (texture(u_NormalMap, vUV).xyz * 2.0 - 1.0));
    }
    vec4 mr = texture(u_MetallicRoughnessTexture, vUV);
    float metallic = clamp(u_Surface.x * mr.b, 0.0, 1.0);
    float roughness = clamp(u_Surface.y * mr.g, 0.04, 1.0);
    float occlusion = mix(1.0, texture(u_OcclusionTexture, vUV).r, clamp(u_Surface.z, 0.0, 1.0));
    float lambert = 0.25 + 0.75 * max(dot(normal, normalize(vec3(0.35, 0.8, 0.45))), 0.0);
    float specular = pow(max(dot(normal, normalize(vec3(0.35, 0.8, 0.45))), 0.0), mix(64.0, 4.0, roughness));
    vec3 emissive = texture(u_EmissiveTexture, vUV).rgb * max(u_Emissive.rgb, vec3(0.0));
    FragColor = vec4((baseColor.rgb * lambert * (1.0 - 0.5 * metallic) + specular * 0.1) * occlusion + emissive, baseColor.a);
    EntityID = u_DrawIds.x;
}
