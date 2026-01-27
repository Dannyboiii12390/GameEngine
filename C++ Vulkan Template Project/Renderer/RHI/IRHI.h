
#include "../ResourcesDescriptors.h"
#include <array>

class BufferDesc;
class TextureDesc;
class ShaderDesc;
class PipelineDesc;
class DescriptorDesc;
class Window;

class IRHI 
{
public:
	virtual void Initialise(Window* window) = 0;
	virtual void Shutdown() = 0;
	virtual void CreateDevice() = 0;
	virtual void CreateSwapchain() = 0;
	virtual void CreateCommandQueue() = 0;
	virtual void CreateCommandBuffer() = 0;
	virtual void CreateBuffer(const RHI::BufferDesc& desc) = 0;
	virtual void CreateTexture(const RHI::TextureDesc& desc) = 0;
	virtual void CreateShader(const RHI::ShaderDesc& desc) = 0;
	virtual void CreatePipelineState(const RHI::PipelineDesc& desc) = 0;
	virtual void CreateDescriptorSet(const RHI::DescriptorDesc& desc) = 0;
	virtual void BeginFrame(const std::array<float, 3> bg_color) = 0;
	virtual void EndFrame() = 0;
	virtual void Present() = 0;
	virtual void WaitIdle() = 0;
};