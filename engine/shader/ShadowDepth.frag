#version 330 core

uniform sampler2D u_BaseColorTexture;

in vec2 vUV;

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
    float alpha = (texture(u_BaseColorTexture, vUV) * u_BaseColorFactor * u_TintColor).a;

    if (u_MaterialFlags.x == 1 && alpha < u_Surface.w)
        discard;
}
