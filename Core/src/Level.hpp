#pragma once

namespace Core
{
	class Level
	{
	public:
		virtual ~Level() {  }
		virtual void OnUpdate() = 0;
		virtual void Shutdown() = 0;
		virtual void Destroy() = 0;
		virtual void Recreate() = 0;
	};
}