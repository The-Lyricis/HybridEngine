#version 330 core

in vec2 v_UV;

layout(location = 0) out vec4 o_Color;

uniform sampler2D u_SceneColorTex;
uniform sampler2D u_SceneDepthTex;
uniform sampler2D u_SelectedMaskTex;
uniform sampler2D u_SelectedDepthTex;
uniform float u_TexelWidth;
uniform float u_TexelHeight;
uniform vec4 u_VisibleOutlineColor;
uniform vec4 u_OccludedOutlineColor;
uniform vec4 u_FillColor;
uniform float u_DepthEpsilon;

float sampleSelectedMask(vec2 uv)
{
    return texture(u_SelectedMaskTex, uv).r;
}

float sampleSceneDepth(vec2 uv)
{
    return texture(u_SceneDepthTex, uv).r;
}

float sampleSelectedDepth(vec2 uv)
{
    return texture(u_SelectedDepthTex, uv).r;
}

bool isInsideProjectedSelection(vec2 uv)
{
    return sampleSelectedMask(uv) > 0.5;
}

void main()
{
    vec4 sceneColor = texture(u_SceneColorTex, v_UV);
    bool centerInSelection = isInsideProjectedSelection(v_UV);
    float centerSceneDepth = sampleSceneDepth(v_UV);
    float centerSelectedDepth = centerInSelection ? sampleSelectedDepth(v_UV) : 1.0;

    // The projected selection mask defines the contour. Depth is only used to
    // classify visible vs occluded styling, not to cut the contour source.
    bool centerVisibleSelection = centerInSelection && centerSelectedDepth <= centerSceneDepth + u_DepthEpsilon;

    vec2 offsets[12] = vec2[](
        vec2(-u_TexelWidth, 0.0),
        vec2( u_TexelWidth, 0.0),
        vec2(0.0, -u_TexelHeight),
        vec2(0.0,  u_TexelHeight),
        vec2(-u_TexelWidth, -u_TexelHeight),
        vec2( u_TexelWidth, -u_TexelHeight),
        vec2(-u_TexelWidth,  u_TexelHeight),
        vec2( u_TexelWidth,  u_TexelHeight),
        vec2(-2.0 * u_TexelWidth, 0.0),
        vec2( 2.0 * u_TexelWidth, 0.0),
        vec2(0.0, -2.0 * u_TexelHeight),
        vec2(0.0,  2.0 * u_TexelHeight)
    );

    bool neighborInSelection = false;
    bool neighborVisibleSelection = false;
    for (int i = 0; i < 12; ++i)
    {
        vec2 sampleUV = clamp(v_UV + offsets[i], vec2(0.0), vec2(1.0));
        if (isInsideProjectedSelection(sampleUV))
        {
            neighborInSelection = true;
            if (sampleSelectedDepth(sampleUV) <= centerSceneDepth + u_DepthEpsilon)
                neighborVisibleSelection = true;
            break;
        }
    }

    vec3 finalColor = sceneColor.rgb;
    if (centerVisibleSelection)
        finalColor = mix(finalColor, u_FillColor.rgb, u_FillColor.a);

    if (!centerInSelection && neighborInSelection)
    {
        vec4 outlineColor = neighborVisibleSelection ? u_VisibleOutlineColor : u_OccludedOutlineColor;
        finalColor = mix(finalColor, outlineColor.rgb, outlineColor.a);
    }

    o_Color = vec4(finalColor, sceneColor.a);
}
