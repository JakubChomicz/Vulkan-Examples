#include "FrameBuffer.h"

namespace Core
{
	static std::vector<FrameBuffer::Attachment> CreateAttachment(vk::Format format)
	{
		std::vector<FrameBuffer::Attachment> ret;
		for (int i = 0; i < Core::Context::_swapchain.swapchainImageViews.size(); ++i)
		{
			FrameBuffer::Attachment temp;
			temp.Format = format;
			vk::ImageCreateInfo imageInfo;
			imageInfo.imageType = vk::ImageType::e2D;
			imageInfo.extent.width = Core::Context::_swapchain.swapchainWidth;
			imageInfo.extent.height = Core::Context::_swapchain.swapchainHeight;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = temp.Format;
			imageInfo.tiling = vk::ImageTiling::eOptimal;
			imageInfo.initialLayout = vk::ImageLayout::eUndefined;
			imageInfo.usage = 
				temp.Format == vk::Format::eD32SfloatS8Uint ? vk::ImageUsageFlagBits::eDepthStencilAttachment :
				temp.Format == vk::Format::eB8G8R8A8Unorm ? vk::ImageUsageFlagBits::eColorAttachment :
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
			imageInfo.sharingMode = vk::SharingMode::eExclusive;
			imageInfo.samples = vk::SampleCountFlagBits::e1;

			temp.Image = Core::Context::_device->createImage(imageInfo);
			temp.MemRequs = Core::Context::_device->getImageMemoryRequirements(temp.Image);
			vk::MemoryAllocateInfo memAlloc;
			memAlloc.sType = vk::StructureType::eMemoryAllocateInfo;
			memAlloc.allocationSize = temp.MemRequs.size;
			memAlloc.memoryTypeIndex = Core::Context::findMemoryType(temp.MemRequs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);		//FIX
			temp.Memory = Core::Context::_device->allocateMemory(memAlloc);
			Core::Context::_device->bindImageMemory(temp.Image, temp.Memory, 0);

			vk::ImageViewCreateInfo viewInfo;
			viewInfo.viewType = vk::ImageViewType::e2D;
			viewInfo.format = temp.Format;
			viewInfo.subresourceRange.aspectMask = temp.Format == vk::Format::eD32SfloatS8Uint ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			viewInfo.image = temp.Image;
			temp.View = Core::Context::_device->createImageView(viewInfo);

			vk::SamplerCreateInfo samplerInfo;
			samplerInfo.sType = vk::StructureType::eSamplerCreateInfo;
			samplerInfo.magFilter = vk::Filter::eLinear;
			samplerInfo.minFilter = vk::Filter::eLinear;
			samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
			samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
			samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
			samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
			samplerInfo.anisotropyEnable = true;
			//samplerInfo.unnormalizedCoordinates = false;
			//samplerInfo.compareEnable = false;
			//samplerInfo.compareOp = vk::CompareOp::eAlways;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.maxAnisotropy = Core::Context::_GPU.getProperties().limits.maxSamplerAnisotropy;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 1.0f;
			samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
			temp.Sampler = Core::Context::_device->createSampler(samplerInfo);

			ret.push_back(temp);
		}
		return ret;
	}

	FrameBuffer::FrameBuffer(vk::RenderPass pass, bool swapChainTarget)
	{
		if(!swapChainTarget)
			m_Attachments.push_back(CreateAttachment(vk::Format::eR32G32B32A32Sfloat));
		m_Attachments.push_back(CreateAttachment(vk::Format::eD32SfloatS8Uint));
		for (int i = 0; i < Core::Context::_swapchain.swapchainImageViews.size(); ++i) {
			std::vector<vk::ImageView> attchs;
			if (swapChainTarget)
			{
				attchs.push_back(Core::Context::_swapchain.swapchainImageViews[i]);
				attchs.push_back(m_Attachments[0][i].View);
			}
			else
			{
				attchs.push_back(m_Attachments[0][i].View);
				attchs.push_back(m_Attachments[1][i].View);
			}
			vk::FramebufferCreateInfo framebufferInfo;
			framebufferInfo.flags = vk::FramebufferCreateFlags();
			framebufferInfo.renderPass = pass;
			framebufferInfo.attachmentCount = attchs.size();
			framebufferInfo.pAttachments = attchs.data();
			framebufferInfo.width = Core::Context::_swapchain.extent.width;
			framebufferInfo.height = Core::Context::_swapchain.extent.height;
			framebufferInfo.layers = 1;
			m_Handles.push_back(Core::Context::_device->createFramebuffer(framebufferInfo));
			Core::Context::WaitIdle();
		}
	}

	FrameBuffer::~FrameBuffer()
	{
	}

	void FrameBuffer::Destroy()
	{
		for (auto framebuffer : m_Handles)
			Core::Context::_device->destroyFramebuffer(framebuffer);
		m_Handles.clear();
		for (auto& frames : m_Attachments)
		{
			for (const auto& frame : frames)
			{
				Core::Context::_device->destroyImageView(frame.View);
				Core::Context::_device->freeMemory(frame.Memory);
				Core::Context::_device->destroyImage(frame.Image);
			}
		}
		m_Attachments.clear();
	}
}

