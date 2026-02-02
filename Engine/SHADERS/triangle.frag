#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 n = normalize(fragNormal);
    // Simple shading: use normal as color (remapped to 0..1)
    vec3 baseColor = n * 0.5 + vec3(0.5);
    outColor = vec4(baseColor, 1.0);
}