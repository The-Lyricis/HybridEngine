#version 330 core

in vec3 vWorldNormal;
in vec2 vUV;

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
    vec4 baseColor = u_BaseColorFactor * u_TintColor;
    if (u_MaterialFlags.x == 1 && baseColor.a < u_Surface.w)
        discard;

    vec3 normal = normalize(vWorldNormal);
    float lambert = 0.25 + 0.75 * max(dot(normal, normalize(vec3(0.35, 0.8, 0.45))), 0.0);
    FragColor = vec4(baseColor.rgb * lambert + max(u_Emissive.rgb, vec3(0.0)), baseColor.a);
    EntityID = u_DrawIds.x;
}
