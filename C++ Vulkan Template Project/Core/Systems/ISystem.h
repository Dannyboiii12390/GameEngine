#pragma once


enum class ESystemType
{
	System_Invalid = 0,
	System_Velocity = 1,
	System_Rendering = 1 << 1
};

class ISystem
{

	ESystemType GetSystemType() const { return m_SystemType; }

private:
	ESystemType m_SystemType = ESystemType::System_Invalid;

};
