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

out vec3 vDirection;

void main()
{
    mat4 view_no_translation = mat4(mat3(u_View));
    vec4 clip = u_Proj * view_no_translation * vec4(aPos, 1.0);
    gl_Position = clip.xyww;
    vDirection = aPos;
}
