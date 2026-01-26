#pragma once


class LightProxy {
public:
	virtual void GetPosition() const = 0;
	virtual void GetType() const = 0;
	virtual void GetDirection() const = 0;
	virtual void GetIntensity() const = 0;
	virtual void CastsShadows() const = 0;
};
