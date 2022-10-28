#pragma once
#include <Level.hpp>
namespace Example
{
	class Monke : public Core::Level
	{
	public:
		Monke();
		~Monke() {  }
		virtual void OnUpdate() override;
		virtual void Shutdown() override;
		virtual void Destroy() override;
		virtual void Recreate() override;
	};
}