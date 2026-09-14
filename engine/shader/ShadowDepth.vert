#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUV;

layout(std140) uniform ShadowViewBlock
{
    mat4 u_LightViewProjection;
};

layout(std140) uniform DrawBlock
{
    mat4 u_Model;
    vec4 u_TintColor;
    uvec4 u_DrawIds;
};

out vec2 vUV;

void main()
{
    vUV = aUV;
    gl_Position = u_LightViewProjection * u_Model * vec4(aPos, 1.0);
}
