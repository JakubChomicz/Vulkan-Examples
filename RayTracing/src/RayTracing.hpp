#pragma once
#include <Level.hpp>

namespace Example
{
	class RayTracing : public Core::Level
	{
	public:
		RayTracing();
		~RayTracing() {  }
		virtual void OnUpdate() override;
		virtual void Shutdown() override;
		virtual void Destroy() override;
		virtual void Recreate() override;
	};
}