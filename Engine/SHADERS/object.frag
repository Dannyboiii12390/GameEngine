#version 450

layout(location = 0) in vec3 fragNormal;   // if vertex outputs a normal at loc 0
layout(location = 1) in vec2 fragUV;      // UV moved to location 1 to match vertex output

layout(location = 0) out vec4 outColor;

// Sampler (set=0 binding=1) - matches RHI descriptor layout
layout(set = 0, binding = 1) uniform sampler2D uTexture;

void main()
{
    vec4 tex = texture(uTexture, fragUV);
    outColor = tex;
}