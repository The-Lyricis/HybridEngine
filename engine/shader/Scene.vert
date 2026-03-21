#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aTangent;

uniform mat4 u_Model;

layout(std140) uniform FrameBlock
{
    mat4 u_View;
    mat4 u_Proj;
    mat4 u_ViewProjection;
    vec4 u_CameraPos;
    vec4 u_Viewport;
};

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec4 vTangent;

void main() {
    mat3 normalMat = transpose(inverse(mat3(u_Model)));

    vWorldPos = vec3(u_Model * vec4(aPos, 1.0));
    vNormal   = normalize(normalMat * aNormal);

    vec3 T = normalize(normalMat * aTangent.xyz);
    vTangent = vec4(T, aTangent.w);

    vUV = aUV;
    gl_Position = u_ViewProjection * vec4(vWorldPos, 1.0);
}
