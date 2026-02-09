#pragma once

#include <GLFW/glfw3.h>
#define GLFW_INCLUDE_VULKAN

//creates an openGL window using GLFW
class Window 
{

public:

	Window(int width, int height, const char* title)
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		GLFWwindow* raw = glfwCreateWindow(width, height, title, nullptr, nullptr);
		m_Window = raw;
	};

	Window(GLFWwindow* window) : m_Window(window)
	{
	}
	GLFWwindow* getGLFWwindow() { return m_Window; }

	void Shutdown()
			{
		if (m_Window)
		{
			glfwDestroyWindow(m_Window);
			m_Window = nullptr;
			glfwTerminate();
		}
	}
	void PollEvents()
	{
		glfwPollEvents();
	}

private:
	GLFWwindow* m_Window;
};