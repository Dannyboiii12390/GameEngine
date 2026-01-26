#pragma once

class IRHICommandBuffer;

class IRHICommandQueue
{
	virtual void Submit(IRHICommandBuffer& buffer) = 0;
	virtual void WaitIdle() = 0;
};