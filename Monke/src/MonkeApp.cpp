#include <App.hpp>
#include <EntryPoint.hpp>
#include "Monke.hpp"

	class MonkeApp : public Core::App
	{
	public:
		MonkeApp(const std::string& name) : Core::App("Monke")
		{
			m_lvl = std::make_shared<Example::Monke>();
		}
		~MonkeApp()
		{

		}
	};

	Core::App* Core::CreateApp(const std::string& name)
	{
		return new MonkeApp(name);
	}