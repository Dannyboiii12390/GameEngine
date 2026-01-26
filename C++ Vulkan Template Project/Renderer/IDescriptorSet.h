#pragma once
class IDescriptor {
public:
	virtual void BindBuffer() = 0;
	virtual void BindTexture() = 0;
	virtual void Update() = 0;
};