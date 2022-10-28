#pragma once
#include <iostream>
#include "Window.hpp"
#include "Level.hpp"

int main(int argc, char** argv);

namespace Core
{
	class App
	{
	public:
		App(const std::string& name = "Example");
		virtual ~App();

		bool m_Maximized = false;
		bool m_Minimalized = false;

		bool OnWindowResize(const uint32_t& width, const uint32_t& height);

		Window& GetWindow() { return *m_Window; }

		void Close();
		static App& Get() { return *s_Instance; }
		std::shared_ptr<Level> m_lvl;
		void Run();
	private:
		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
	private:
		static App* s_Instance;
		friend class Window;
		friend int::main(int argc, char** argv);
	};

	App* CreateApp(const std::string& name);
}