#include <App.hpp>
#include <EntryPoint.hpp>
#include "RayTracing.hpp"

	class RayTracingApp : public Core::App
	{
	public:
		RayTracingApp(const std::string& name) : Core::App("RayTracing")
		{
			m_lvl = std::make_shared<Example::RayTracing>();
		}
		~RayTracingApp()
		{

		}
	};

	Core::App* Core::CreateApp(const std::string& name)
	{
		return new RayTracingApp(name);
	}