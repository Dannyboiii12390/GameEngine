#include <cstdint>


class IBuffer
{
public:
	virtual uint32_t GetSize() const = 0;
	virtual void map() = 0;
	virtual void unmap() = 0;
};
