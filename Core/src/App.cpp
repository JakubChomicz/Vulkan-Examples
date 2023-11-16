#include "pch.hpp"
#include "App.hpp"
#include "API/VulkanInit.hpp"

namespace Core
{
	App* App::s_Instance = nullptr;

	App::App(const std::string& name)
	{
		s_Instance = this;
		m_Window = std::make_unique<Window>();
		//Selecting API
		{
			Context::Init();
		}
		//Window Creation
		{
			m_Window->Init(name, 1600, 900);
		}
		//Rendering Interface Initialization
		{
			Context::Get()->SetWindowSurface(m_Window->GetNativeWindow());
			Context::Get()->InitDevice();
			Context::Get()->CreateSwapchain(m_Window->GetWidth(), m_Window->GetHeight());
			Context::Get()->CreateSwapchainImages();
		}
	}

	App::~App()
	{
		Context::Get()->GetDevice()->waitIdle();
		m_lvl->Shutdown();
		Context::Shutdown();
	}
	void App::Run()
	{
		while (!m_Window->ShouldClose())
		{
			m_Window->OnUpdate();
			if (!m_Window->IsMinimized())
			{
				if (m_Window->getWindowData().Width > 0 && m_Window->getWindowData().Height > 0)
				{
					Context::Get()->AcquireNextSwapchainImage();
					m_lvl->OnUpdate();
					Context::Get()->Present();
				}
			}
		}
	}
	void App::Close()
	{
		m_Running = false;
	}
	bool App::OnWindowResize(const uint32_t& width, const uint32_t& height)
	{
		if (width == 0 || height == 0)
			return false;
		Context::Get()->WaitIdle();
		m_lvl->Destroy();
		Context::Get()->DestroySwapchain();
		Context::Get()->RecreateSwapchain(width, height);
		m_lvl->Recreate();

		return false;
	}
}