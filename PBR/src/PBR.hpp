#pragma once
#include <Level.hpp>

namespace Example
{
	class PBR : public Core::Level
	{
	public:
		PBR();
		~PBR() {  }
		virtual void OnUpdate() override;
		virtual void Shutdown() override;
		virtual void Destroy() override;
		virtual void Recreate() override;
	};
}