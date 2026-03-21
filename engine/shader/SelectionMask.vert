#version 330 core

layout(location = 0) in vec3 aPos;

layout(std140) uniform FrameBlock
{
    mat4 u_View;
    mat4 u_Proj;
    mat4 u_ViewProjection;
    vec4 u_CameraPos;
    vec4 u_Viewport;
};

uniform mat4 u_Model;

void main()
{
    gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
}
