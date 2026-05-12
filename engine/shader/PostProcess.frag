#version 330 core

in vec2 v_UV;

layout(location = 0) out vec4 o_Color;

uniform sampler2D u_SceneColorTex;
uniform int u_EnableToneMapping;
uniform int u_EnableGammaCorrection;
uniform float u_Exposure;
uniform float u_Gamma;

vec3 applyToneMapping(vec3 color)
{
    vec3 exposed = color * max(u_Exposure, 0.0);
    return vec3(1.0) - exp(-exposed);
}

vec3 applyGammaCorrection(vec3 color)
{
    float gamma = max(u_Gamma, 0.0001);
    return pow(max(color, vec3(0.0)), vec3(1.0 / gamma));
}

void main()
{
    vec4 sceneColor = texture(u_SceneColorTex, v_UV);
    vec3 color = sceneColor.rgb;

    if (u_EnableToneMapping != 0)
        color = applyToneMapping(color);
    if (u_EnableGammaCorrection != 0)
        color = applyGammaCorrection(color);

    o_Color = vec4(color, sceneColor.a);
}
