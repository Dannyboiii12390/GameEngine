


class Window;
class IScene;

class IRenderer
{
public:
	virtual void Initialise(Window& Window) = 0;
	virtual void Shutdown() = 0;
	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;
	virtual void RenderFrame(const IScene& scene) = 0;
	virtual void Resize(int width, int height) = 0;
	virtual void WaitForGPU() = 0;
};

