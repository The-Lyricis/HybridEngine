#version 330 core

in vec3 vDirection;

layout(location = 0) out vec4 FragColor;

uniform samplerCube u_SkyboxCubemap;
uniform float u_Intensity;
uniform float u_RotationDegrees;

mat3 rotationY(float radians_value)
{
    float c = cos(radians_value);
    float s = sin(radians_value);
    return mat3(
         c, 0.0, s,
       0.0, 1.0, 0.0,
        -s, 0.0, c
    );
}

void main()
{
    vec3 dir = normalize(rotationY(radians(u_RotationDegrees)) * vDirection);
    vec3 color = texture(u_SkyboxCubemap, dir).rgb * max(u_Intensity, 0.0);
    FragColor = vec4(color, 1.0);
}
