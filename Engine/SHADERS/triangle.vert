#version 450

// push-constant model matrix (column-major)
layout(push_constant) uniform PushConstants {
    mat4 uModel;
} pc;

// Match Mesh::Vertex attribute layout exactly
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragColor;

void main() {
    // transform position by per-object model matrix
    gl_Position = pc.uModel * vec4(inPos, 1.0);

    // simple color from normal for debugging (or hardcoded)
    fragColor = normalize(inNormal) * 0.5 + vec3(0.5);
}