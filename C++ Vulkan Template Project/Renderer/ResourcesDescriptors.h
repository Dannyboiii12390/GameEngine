
#pragma once

// Graphics-API agnostic resource / pipeline / descriptor descriptions
// Designed to be a minimal, extensible set of POD-like descriptors that
// any backend (Vulkan, D3D12, Metal, GL) can consume/translate.

// Keep includes minimal to avoid heavy dependencies for RHI headers.
#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace RHI
{

    // Generic usage hints for memory / lifetime
    enum class ResourceUsage : uint8_t
    {
        Default,    // GPU local, optimal for GPU access
        Immutable,  // Never updated after creation
        Dynamic,    // Updated frequently by CPU
        Staging     // CPU-visible staging resource
    };

    // Buffer types consumers typically need
    enum class BufferType : uint8_t
    {
        Unknown,
        Vertex,
        Index,
        Uniform,    // constant buffer / UBO
        Storage,    // SSBO / UAV
        Indirect,
        Staging
    };

    struct BufferDesc
    {
        uint64_t        Size = 0;                // size in bytes
        BufferType      Type = BufferType::Unknown;
        ResourceUsage   Usage = ResourceUsage::Default;
        uint32_t        Stride = 0;              // element stride for structured/vertex buffers
        bool            CpuMappable = false;     // can CPU map
        std::string     DebugName;               // optional name for debug/profiling

        BufferDesc() = default;
    };

    // Common, portable texture / image formats (expand as needed)
    enum class TextureFormat : uint32_t
    {
        Unknown = 0,
        R8_UNORM,
        RG8_UNORM,
        RGBA8_UNORM,
        BGRA8_UNORM,
        R8G8B8A8_SRGB,
        R16_FLOAT,
        RG16_FLOAT,
        RGBA16_FLOAT,
        RGBA32_FLOAT,
        Depth24Stencil8,
        Depth32_FLOAT
    };

    // Texture usage flags (bitmask stored in uint32_t)
    enum TextureUsage : uint32_t
    {
        TextureUsage_None = 0u,
        TextureUsage_Sampled = 1u << 0,
        TextureUsage_ColorAttachment = 1u << 1,
        TextureUsage_DepthStencil = 1u << 2,
        TextureUsage_TransferSrc = 1u << 3,
        TextureUsage_TransferDst = 1u << 4,
        TextureUsage_Storage = 1u << 5
    };

    struct TextureDesc
    {
        uint32_t        Width = 0;
        uint32_t        Height = 0;
        uint32_t        Depth = 1;
        uint32_t        ArrayLayers = 1;
        uint32_t        MipLevels = 1;
        TextureFormat   Format = TextureFormat::Unknown;
        uint32_t        Samples = 1;            // sample count (1 = no MSAA)
        uint32_t        Usage = TextureUsage_None; // combination of TextureUsage bits
        bool            IsCubemap = false;
        ResourceUsage   MemUsage = ResourceUsage::Default;
        std::string     DebugName;

        TextureDesc() = default;
    };

    // Shader stages
    enum class ShaderStage : uint8_t
    {
        Vertex,
        Fragment,
        Compute,
        Geometry,
        TessControl,
        TessEvaluation
    };

    // Small bitmask helpers for shader stage visibility in descriptor bindings
    enum ShaderStageFlags : uint32_t
    {
        ShaderStageFlags_None = 0u,
        ShaderStageFlags_Vertex = 1u << 0,
        ShaderStageFlags_Fragment = 1u << 1,
        ShaderStageFlags_Compute = 1u << 2,
        ShaderStageFlags_Geometry = 1u << 3,
        ShaderStageFlags_TessCtrl = 1u << 4,
        ShaderStageFlags_TessEval = 1u << 5,
        ShaderStageFlags_All = 0xFFFFFFFFu
    };

    // Shader description (API-agnostic)
    // - backends may accept SPIR-V/bytecode in `Bytecode` or load from `SourceFile`
    struct ShaderDesc
    {
        ShaderStage             Stage = ShaderStage::Vertex;
        std::string             EntryPoint = "main";
        std::vector<uint8_t>    Bytecode;       // optional binary (SPIR-V, DXIL, etc.)
        std::string             SourceFile;     // optional path to shader source
        std::string             DebugName;

        ShaderDesc() = default;
    };

    // High-level vertex attribute description (API agnostic)
    struct VertexAttribute
    {
        uint32_t    Location = 0;     // shader location / semantic index
        uint32_t    Format = 0;       // backend-specific format enum value (use small mapping)
        uint32_t    Offset = 0;       // byte offset within vertex
        std::string Semantic;         // optional semantic/name
    };

    // Vertex input layout
    struct VertexLayout
    {
        uint32_t                            Stride = 0;
        std::vector<VertexAttribute>        Attributes;
    };

    // Primitive topology (API-agnostic)
    enum class PrimitiveTopology : uint8_t
    {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        PatchList
    };

    // Simple rasterization / depth / blend state summary (extend later as needed)
    struct RasterState
    {
        bool                CullEnable = false;
        enum class CullMode : uint8_t { None = 0, Front, Back } Cull = CullMode::Back;
        bool                DepthTest = true;
        bool                DepthWrite = true;
        bool                BlendEnable = false;
    };

    // Pipeline description (graphics / compute)
    // - For graphics pipelines, fill shader names/IDs for vs/fs etc. Backends map `ShaderId*` to actual shader objects.
    struct PipelineDesc
    {
        std::string         DebugName;

        // shader "identifiers" - engine may resolve these strings to actual Shader objects
        std::string         VS;     // vertex shader id/name
        std::string         FS;     // fragment shader id/name (empty for compute)
        std::string         CS;     // compute shader id/name (empty for graphics)

        VertexLayout        VertexLayoutDesc;
        PrimitiveTopology   Topology = PrimitiveTopology::TriangleList;
        RasterState         Raster;
        bool                DynamicViewport = true; // whether viewport/scissor are dynamic
        // descriptor set layout references, push-constant sizes, render target formats, etc. can be added later

        PipelineDesc() = default;
    };

    // Descriptor / descriptor-set layout description
    enum class DescriptorType : uint8_t
    {
        Sampler,
        CombinedImageSampler,
        SampledImage,
        StorageImage,
        UniformTexelBuffer,
        StorageTexelBuffer,
        UniformBuffer,
        StorageBuffer,
        InputAttachment
    };

    struct DescriptorBinding
    {
        uint32_t            Binding = 0;
        DescriptorType      Type = DescriptorType::UniformBuffer;
        uint32_t            Count = 1;                 // array size, typically 1
        uint32_t            StageFlags = ShaderStageFlags_All;
        std::string         DebugName;
    };

    struct DescriptorDesc
    {
        // A single descriptor set layout / signature.
        // Backends should translate into the native descriptor set layout for allocation.
        std::vector<DescriptorBinding>    Bindings;
        std::string                       DebugName;

        DescriptorDesc() = default;
    };

} // namespace RHI