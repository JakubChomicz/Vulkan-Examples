#include <App.hpp>
#include <EntryPoint.hpp>
#include "Cube.hpp"

	class CubeApp : public Core::App
	{
	public:
		CubeApp(const std::string& name) : Core::App("Cube")
		{
			m_lvl = std::make_shared<Example::Cube>();
		}
		~CubeApp()
		{

		}
	};

	Core::App* Core::CreateApp(const std::string& name)
	{
		return new CubeApp(name);
	}