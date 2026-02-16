#pragma once


enum class ESystemType
{
	System_Invalid = 0,
	System_Velocity = 1,
	System_Rendering = 1 << 1,
	System_Physics = 1 << 2
};

class ISystem
{
public:

	ESystemType GetSystemType() const { return m_SystemType; }

protected:
	ESystemType m_SystemType = ESystemType::System_Invalid;

};
