#include <App.hpp>
#include <EntryPoint.hpp>
#include "PBR.hpp"

	class PBRApp : public Core::App
	{
	public:
		PBRApp(const std::string& name) : Core::App("PBR")
		{
			m_lvl = std::make_shared<Example::PBR>();
		}
		~PBRApp()
		{

		}
	};

	Core::App* Core::CreateApp(const std::string& name)
	{
		return new PBRApp(name);
	}