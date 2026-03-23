#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUV;

uniform mat4 u_Model;
uniform mat4 u_LightViewProjection;

out vec2 vUV;

void main()
{
    vUV = aUV;
    gl_Position = u_LightViewProjection * u_Model * vec4(aPos, 1.0);
}
