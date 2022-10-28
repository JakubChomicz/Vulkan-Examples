#include "pch.hpp"
#include "Window.hpp"
#include "App.hpp"
#include "API/VulkanInit.hpp"

namespace Core
{
	static uint8_t s_GLFWWindowCount = 0;

	static void GLFWErrorCallback(int error, const char* description)
	{
		std::cout << "GLFW Error (" << error << "): " << description << std::endl;
	}

	Window::Window()
	{
		//GLFW Initialization
		if (s_GLFWWindowCount == 0)
		{
			int success = glfwInit();
			success ? std::cout << "GLFW initialized Correctly!" << std::endl : std::cout << "Could not initialize GLFW!" << std::endl;
			glfwSetErrorCallback(GLFWErrorCallback);
		}
	}
	Window::~Window()
	{
		Shutdown();
	}
	void Window::Init(const std::string& title, uint32_t width, uint32_t height)
	{
		m_Data.Title = title;
		m_Data.Width = width;
		m_Data.Height = height;

		std::cout << "Creating window " << title << " (" << width << ", " << height << ")" << std::endl;

		//Creating Window and makeing sure that it's size is correct
		{
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
			std::string windowTitle = m_Data.Title;
			m_Window = glfwCreateWindow((int)m_Data.Width, (int)m_Data.Height, windowTitle.c_str(), nullptr, nullptr);
			glfwSetWindowSizeLimits(m_Window, 800, 600, GLFW_DONT_CARE, GLFW_DONT_CARE);
			++s_GLFWWindowCount;
			int w, h;
			glfwGetWindowSize(m_Window, &w, &h);
			m_Data.Width = w;
			m_Data.Height = h;
		}

		glfwSetWindowUserPointer(m_Window, &m_Data);//Setting Window Ptr

		//Callbacks
		{
			glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
				{
					WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
					data.Width = width;
					data.Height = height;
					App::Get().OnWindowResize(width, height);
				});
		}
	}
	void Window::Shutdown()
	{
		glfwDestroyWindow(m_Window);
		--s_GLFWWindowCount;

		if (s_GLFWWindowCount == 0)
		{
			glfwTerminate();
		}
	}
	void Window::OnUpdate()
	{
		glfwPollEvents();
	}
	bool Window::IsMaximized()
	{
		return static_cast<bool>(glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED));
	}
	bool Window::IsMinimized()
	{
		return static_cast<bool>(glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED));
	}
	std::pair<float, float> Window::GetWindowCenter()
	{
		int x, y;
		glfwGetWindowPos(m_Window, &x, &y);

		return std::pair<float, float>({ x + m_Data.Width/2, y + m_Data.Height/2 });
	}
}