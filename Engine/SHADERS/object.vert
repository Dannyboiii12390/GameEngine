#version 450

// Camera UBO: view then projection (matches VulkanRHI UpdateCameraBuffer)
layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} ubo;

// Push-constant model matrix (set by renderer)
layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

// Vertex inputs (match Mesh::Vertex attribute layout)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// Outputs to fragment shader
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;

void main()
{
    mat4 mvp = ubo.proj * ubo.view * pc.model;
    gl_Position = mvp * vec4(inPosition, 1.0);

    // Transform normal by model (assumes no non-uniform scale).
    fragNormal = mat3(pc.model) * inNormal;
    fragUV = inUV;
}