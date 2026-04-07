

#pragma once
#include <type_traits>
#include <cstdint>

enum class EComponentType : uint32_t
{
	Component_None = 0,
	Component_Transform = 1u << 0,
	Component_Velocity = 1u << 1,
	Component_Geometry = 1u << 2,
	Component_Audio = 1u << 3,
	Component_Physics = 1u << 4,
	Component_Collision = 1u << 5,
	Component_Network = 1u << 6
};
inline const uint32_t to_mask(EComponentType type) noexcept
{
	return static_cast<std::underlying_type_t<EComponentType>>(type);
}
// Bitwise operators for EComponentType
// OR
inline constexpr EComponentType operator|(EComponentType a, EComponentType b) noexcept
{
	using T = std::underlying_type_t<EComponentType>;
	return static_cast<EComponentType>(static_cast<T>(a) | static_cast<T>(b));
}
inline constexpr EComponentType& operator|=(EComponentType& a, EComponentType b) noexcept
{
	a = a | b;
	return a;
}

// AND
inline constexpr EComponentType operator&(EComponentType a, EComponentType b) noexcept
{
	using T = std::underlying_type_t<EComponentType>;
	return static_cast<EComponentType>(static_cast<T>(a) & static_cast<T>(b));
}
inline constexpr EComponentType& operator&=(EComponentType& a, EComponentType b) noexcept
{
	a = a & b;
	return a;
}

class IComponent
{
public:
	IComponent() = default;
	IComponent(EComponentType type)
		: m_Type(type)
	{
	}
	// Make the base polymorphic so dynamic_cast works.
	// Also ensure proper cleanup via virtual dtor.
	virtual ~IComponent() = default;

	EComponentType GetType() const noexcept { return m_Type; }

private:
	EComponentType m_Type = EComponentType::Component_None;
};