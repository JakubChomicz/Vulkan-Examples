#include "RenderPass.h"

namespace Core
{
	RenderPass::RenderPass(bool HDR, bool useDepthAttachment)
	{
		vk::AttachmentDescription colorAttachment{};
		colorAttachment.flags = vk::AttachmentDescriptionFlags();
		colorAttachment.format = HDR ? vk::Format::eR32G32B32A32Sfloat : vk::Format::eB8G8R8A8Unorm;
		colorAttachment.samples = vk::SampleCountFlagBits::e1;
		colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
		colorAttachment.finalLayout = HDR ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::ePresentSrcKHR;

		vk::AttachmentDescription depthAttachment{};
		depthAttachment.flags = vk::AttachmentDescriptionFlags();
		depthAttachment.format = vk::Format::eD32SfloatS8Uint;
		depthAttachment.samples = vk::SampleCountFlagBits::e1;
		depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
		depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		//Declare that attachment to be color buffer 0 of the framebuffer
		vk::AttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::AttachmentReference depthStencilAttachmentRef = {};
		depthStencilAttachmentRef.attachment = 1;
		depthStencilAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		//Renderpasses are broken down into subpasses, there's always at least one.
		vk::SubpassDescription subpass{};
		subpass.flags = vk::SubpassDescriptionFlags();
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = useDepthAttachment ? &depthStencilAttachmentRef : nullptr;

		std::vector<vk::AttachmentDescription> attachments{ colorAttachment };
		if(useDepthAttachment) 
			attachments.push_back(depthAttachment);

		//Now create the renderpass
		vk::RenderPassCreateInfo renderpassInfo{};
		renderpassInfo.flags = vk::RenderPassCreateFlags();
		renderpassInfo.attachmentCount = attachments.size();
		renderpassInfo.pAttachments = attachments.data();
		renderpassInfo.subpassCount = 1;
		renderpassInfo.pSubpasses = &subpass;
		m_Handle = Core::Context::_device->createRenderPass(renderpassInfo);
	}

	RenderPass::~RenderPass()
	{
	}

	void RenderPass::Destroy()
	{
		Core::Context::_device->destroyRenderPass(m_Handle);
	}
}
