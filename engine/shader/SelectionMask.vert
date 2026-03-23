#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUV;

layout(std140) uniform FrameBlock
{
    mat4 u_View;
    mat4 u_Proj;
    mat4 u_ViewProjection;
    vec4 u_CameraPos;
    vec4 u_Viewport;
};

uniform mat4 u_Model;

out vec2 vUV;

void main()
{
    vUV = aUV;
    gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
}
