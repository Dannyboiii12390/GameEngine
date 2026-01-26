#pragma once
#include <cstdint>

class ITexture
{
public:
	virtual uint32_t GetWidth() const = 0;
	virtual uint32_t GetHeight() const = 0;
	virtual uint32_t GetFormat() const = 0;
};
