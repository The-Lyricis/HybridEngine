#version 330 core

in vec2 vUV;

uniform vec4 u_AlbedoColor;
uniform vec4 u_TintColor;
uniform sampler2D u_AlbedoMap;
uniform int u_SurfaceMode;
uniform float u_AlphaCutoff;

void main()
{
    vec4 albedo_sample = texture(u_AlbedoMap, vUV);
    vec4 albedo_tint = u_AlbedoColor * u_TintColor;
    float alpha = albedo_tint.a * albedo_sample.a;

    if (u_SurfaceMode == 1 && alpha < u_AlphaCutoff)
        discard;
}
