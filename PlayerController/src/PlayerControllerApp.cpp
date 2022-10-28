#include <App.hpp>
#include <EntryPoint.hpp>
#include "PlayerController.hpp"

	class PlayerControllerApp : public Core::App
	{
	public:
		PlayerControllerApp(const std::string& name) : Core::App("PlayerController")
		{
			m_lvl = std::make_shared<Example::PlayerController>();
		}
		~PlayerControllerApp()
		{

		}
	};

	Core::App* Core::CreateApp(const std::string& name)
	{
		return new PlayerControllerApp(name);
	}