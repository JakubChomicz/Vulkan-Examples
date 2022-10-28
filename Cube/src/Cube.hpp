#pragma once
#include <Level.hpp>
namespace Example
{
	class Cube : public Core::Level
	{
	public:
		Cube();
		~Cube() {  }
		virtual void OnUpdate() override;
		virtual void Shutdown() override;
		virtual void Destroy() override;
		virtual void Recreate() override;
	};
}