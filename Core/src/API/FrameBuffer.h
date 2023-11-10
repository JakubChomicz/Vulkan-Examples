#pragma once
#include "VulkanInit.hpp"
namespace Core
{

	class FrameBuffer
	{
	public:
		FrameBuffer(vk::RenderPass pass, bool swapChainTarget = false);
		virtual ~FrameBuffer();
		void Destroy();

		const vk::Framebuffer GetFrameBuffer() const { return m_Handles[Core::Context::_swapchain.currentFrame]; }

		const uint32_t GetFramesInFlightCount() const { return m_Handles.size(); }

		struct Attachment
		{
			vk::Format Format;
			vk::Image Image;
			vk::ImageView View;
			vk::Sampler Sampler;
			vk::MemoryRequirements MemRequs;
			vk::DeviceMemory Memory;
		};

		const FrameBuffer::Attachment GetFrameBufferAttachment(uint32_t frame, uint32_t index) { return m_Attachments[index][frame]; }
	private:
		std::vector<std::vector<Attachment>> m_Attachments;
		std::vector<vk::Framebuffer> m_Handles;
	};
}