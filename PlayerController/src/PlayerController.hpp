#pragma once
#include <Level.hpp>
namespace Example
{
	class PlayerController : public Core::Level
	{
	public:
		PlayerController();
		~PlayerController() {  }
		virtual void OnUpdate() override;
		virtual void Shutdown() override;
		virtual void Destroy() override;
		virtual void Recreate() override;
	};
}