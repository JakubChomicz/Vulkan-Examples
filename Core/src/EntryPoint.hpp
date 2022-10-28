#include <iostream>
#include "App.hpp"

extern Core::App* Core::CreateApp(const std::string& name);

int main(int argc, char** argv)
{
	auto app = Core::CreateApp("App");
	app->Run();
	delete app;
}
