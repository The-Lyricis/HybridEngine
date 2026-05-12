#version 330 core

in vec2 vUV;

uniform vec4 u_BaseColorFactor;
uniform vec4 u_TintColor;
uniform sampler2D u_BaseColorTexture;
uniform int u_AlphaMode;
uniform float u_AlphaCutoff;

void main()
{
    vec4 albedo_sample = texture(u_BaseColorTexture, vUV);
    vec4 albedo_tint = u_BaseColorFactor * u_TintColor;
    float alpha = albedo_tint.a * albedo_sample.a;

    if (u_AlphaMode == 1 && alpha < u_AlphaCutoff)
        discard;
}
