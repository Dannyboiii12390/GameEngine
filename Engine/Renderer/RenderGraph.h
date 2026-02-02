
#include <vector>
#include "RenderPass/IRenderPass.h"


class RenderGraph
{
public:
	explicit RenderGraph() = default;

	void AddPass(IRenderPass&& pass)
	{
		m_Passes.push_back(std::move(pass));
	}
	void Execute(RHICommandBuffer& cmd)
	{
		for (auto& pass : m_Passes)
		{
			pass.Execute(cmd);
		}
	}
	void RemovePass(const int index) 
	{
		if (index >= 0 && index < static_cast<int>(m_Passes.size()))
		{
			m_Passes.erase(m_Passes.begin() + index);
		}
	}
	void ClearPasses()
	{
		m_Passes.clear();
	}

private:
	std::vector<IRenderPass> m_Passes;

};