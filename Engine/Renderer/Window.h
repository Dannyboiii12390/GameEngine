#pragma once

#include <GLFW/glfw3.h>
#define GLFW_INCLUDE_VULKAN

//creates an openGL window using GLFW
class Window 
{

public:
	Window(GLFWwindow* window)
		: m_Window(window)
	{
	}
	GLFWwindow* getGLFWwindow() { return m_Window; }


private:
	GLFWwindow* m_Window;
};