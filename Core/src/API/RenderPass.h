#pragma once
#include "VulkanInit.hpp"

namespace Core
{
	class RenderPass
	{
	public:
		RenderPass(bool HDR = true, bool useDepthAttachment = true);
		virtual ~RenderPass();
		void Destroy();

		vk::RenderPass GetRenderPass() { return m_Handle; }

	private:
		vk::RenderPass m_Handle;
	};
}