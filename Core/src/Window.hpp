#pragma once
#include "pch.hpp"
#include <GLFW/glfw3.h>

namespace Core
{
	class Window
	{
	public:
		Window();
		virtual ~Window();

		void OnUpdate();

		inline unsigned int GetWidth() const { return m_Data.Width; }
		inline unsigned int GetHeight() const { return m_Data.Height; }

		inline void* GetNativeWindow() const { return m_Window; }

		void Init(const std::string& title, uint32_t width, uint32_t height);
		int ShouldClose() { return glfwWindowShouldClose(m_Window); }
		bool IsMaximized();
		bool IsMinimized();
		std::pair<float, float> GetWindowCenter();
	private:
		virtual void Shutdown();
	private:
		GLFWwindow* m_Window;
		bool maximalized = false;
		struct WindowData
		{
			std::string Title;
			unsigned int Width, Height;
			bool VSync;
		};

		WindowData m_Data;
	public:
		const WindowData& getWindowData() { return m_Data; }
	};
}
