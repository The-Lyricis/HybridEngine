#version 330 core

in vec2 v_UV;

layout(location = 0) out vec4 o_Color;

uniform usampler2D u_EntityIDTex;
uniform uint u_SelectedEntityID;
uniform float u_TexelWidth;
uniform float u_TexelHeight;
uniform vec4 u_OutlineColor;

uint sample_id(vec2 uv)
{
    return texture(u_EntityIDTex, uv).r;
}

void main()
{
    if (v_UV.x <= 2.0 * u_TexelWidth ||
        v_UV.x >= 1.0 - 2.0 * u_TexelWidth ||
        v_UV.y <= 2.0 * u_TexelHeight ||
        v_UV.y >= 1.0 - 2.0 * u_TexelHeight)
    {
        discard;
    }

    uint center_id = sample_id(v_UV);
    bool self_selected = center_id == u_SelectedEntityID;

    vec2 offsets[16] = vec2[](
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
        vec2(0.0,  2.0 * u_TexelHeight),
        vec2(-2.0 * u_TexelWidth, -2.0 * u_TexelHeight),
        vec2( 2.0 * u_TexelWidth, -2.0 * u_TexelHeight),
        vec2(-2.0 * u_TexelWidth,  2.0 * u_TexelHeight),
        vec2( 2.0 * u_TexelWidth,  2.0 * u_TexelHeight)
    );

    bool neighbor_selected = false;
    for (int i = 0; i < 16; ++i)
    {
        vec2 uv = clamp(v_UV + offsets[i], vec2(0.0), vec2(1.0));
        if (sample_id(uv) == u_SelectedEntityID)
        {
            neighbor_selected = true;
            break;
        }
    }

    if (!self_selected && neighbor_selected)
    {
        o_Color = u_OutlineColor;
        return;
    }

    discard;
}
