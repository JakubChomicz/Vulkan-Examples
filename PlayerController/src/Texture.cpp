#include "Texture.hpp"
#include <stb_image/stb_image.h>

namespace Example
{
    void Texture::Destroy()
    {
        Core::Context::_device->freeDescriptorSets(Core::Context::_descriptorPool, m_set);
        Core::Context::_device->destroyDescriptorSetLayout(m_layout);
        Core::Context::_device->destroyBuffer(m_buffer);
        Core::Context::_device->destroyImageView(m_view);
        Core::Context::_device->destroySampler(m_sampler);
        Core::Context::_device->destroyImage(m_img);
        Core::Context::_device->freeMemory(m_mem);
    }
    void Texture::load(const std::string& path)
	{
        vk::CommandBuffer transCmd;
        vk::Buffer tempBuffer;
        vk::DeviceMemory tempmemory;
        int width, height, channels;

        stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);  //setting up that the image will allways have 4 channels
        vk::DeviceSize imgSize = width * height * 4;    //4 are channels
        //m_Width = width;
        //m_Height = height;

        //Create Image
        {
            vk::ImageCreateInfo imgInfo;
            imgInfo.sType = vk::StructureType::eImageCreateInfo;
            imgInfo.imageType = vk::ImageType::e2D;
            imgInfo.extent.width = width;
            imgInfo.extent.height = height;
            imgInfo.extent.depth = 1;
            imgInfo.mipLevels = 1;
            imgInfo.arrayLayers = 1;
            imgInfo.format = vk::Format::eR8G8B8A8Srgb;
            imgInfo.tiling = vk::ImageTiling::eOptimal;
            imgInfo.initialLayout = vk::ImageLayout::eUndefined;
            imgInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
            imgInfo.samples = vk::SampleCountFlagBits::e1;
            imgInfo.sharingMode = vk::SharingMode::eExclusive;

            m_img = Core::Context::_device->createImage(imgInfo);

            vk::MemoryRequirements memReqs = Core::Context::_device->getImageMemoryRequirements(m_img);
            vk::MemoryAllocateInfo memAlloc;
            memAlloc.sType = vk::StructureType::eMemoryAllocateInfo;
            memAlloc.allocationSize = memReqs.size;
            memAlloc.memoryTypeIndex = Core::Context::findMemoryType(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);		//FIX
            m_mem = Core::Context::_device->allocateMemory(memAlloc);
            Core::Context::_device->bindImageMemory(m_img, m_mem, /*vk::DeviceSize*/0);
        }
        //Create Image View
        {
            vk::ImageViewCreateInfo imgViewInfo;
            imgViewInfo.sType = vk::StructureType::eImageViewCreateInfo;
            imgViewInfo.viewType = vk::ImageViewType::e2D;
            imgViewInfo.format = vk::Format::eR8G8B8A8Srgb;
            imgViewInfo.subresourceRange = vk::ImageSubresourceRange({});
            imgViewInfo.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eColor;
            imgViewInfo.subresourceRange.baseMipLevel = 0;
            imgViewInfo.subresourceRange.levelCount = 1;
            imgViewInfo.subresourceRange.baseArrayLayer = 0;
            imgViewInfo.subresourceRange.layerCount = 1;
            imgViewInfo.image = m_img;
            m_view = Core::Context::_device->createImageView(imgViewInfo);
        }
        //Create Sampler
        {
            vk::SamplerCreateInfo samplerInfo;
            samplerInfo.sType = vk::StructureType::eSamplerCreateInfo;
            samplerInfo.magFilter = vk::Filter::eLinear;
            samplerInfo.minFilter = vk::Filter::eLinear;
            samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
            samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
            samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
            samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
            samplerInfo.anisotropyEnable = true;
            samplerInfo.unnormalizedCoordinates = false;
            samplerInfo.compareEnable = false;
            samplerInfo.compareOp = vk::CompareOp::eAlways;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.maxAnisotropy = Core::Context::_GPU.getProperties().limits.maxSamplerAnisotropy;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 0.0f;
            samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
            m_sampler = Core::Context::_device->createSampler(samplerInfo);
        }
        // Create the Upload Buffer:
        {
            vk::BufferCreateInfo info;
            info.sType = vk::StructureType::eBufferCreateInfo;
            info.size = imgSize;
            info.usage = vk::BufferUsageFlagBits::eTransferSrc;
            info.sharingMode = vk::SharingMode::eExclusive;
            tempBuffer = Core::Context::_device->createBuffer(info);
            auto memReq = Core::Context::_device->getBufferMemoryRequirements(tempBuffer);
            vk::MemoryAllocateInfo allocate;
            allocate.sType = vk::StructureType::eMemoryAllocateInfo;
            allocate.allocationSize = memReq.size;
            vk::MemoryPropertyFlags flags = vk::MemoryPropertyFlagBits::eHostVisible;
            allocate.memoryTypeIndex = Core::Context::findMemoryType(memReq.memoryTypeBits, flags);
            tempmemory = Core::Context::_device->allocateMemory(allocate);
            Core::Context::_device->bindBufferMemory(tempBuffer, tempmemory, 0);
            /// /
            auto data = Core::Context::_device->mapMemory(tempmemory, 0, imgSize);     //FIX
            memcpy(data, pixels, static_cast<size_t>(imgSize));
            Core::Context::_device->unmapMemory(tempmemory);
        }
        stbi_image_free(pixels);

        // Copy to Image:
        {
            vk::CommandBufferAllocateInfo allocinfo;
            allocinfo.sType = vk::StructureType::eCommandBufferAllocateInfo;
            allocinfo.level = vk::CommandBufferLevel::ePrimary;
            allocinfo.commandPool = Core::Context::_graphicsCommandPool;
            allocinfo.commandBufferCount = 1;
            auto tempVecCmd = Core::Context::_device->allocateCommandBuffers(allocinfo);
            transCmd = tempVecCmd.front();

            vk::CommandBufferBeginInfo beg;
            beg.sType = vk::StructureType::eCommandBufferBeginInfo;
            beg.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

            transCmd.begin(beg);
            vk::ImageMemoryBarrier copy_barrier;
            copy_barrier.sType = vk::StructureType::eImageMemoryBarrier;
            copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
            copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
            copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier.image = m_img;
            copy_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            copy_barrier.subresourceRange.levelCount = 1;
            copy_barrier.subresourceRange.layerCount = 1;
            transCmd.pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &copy_barrier);

            vk::BufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;

            region.imageOffset.x = 0;
            region.imageOffset.y = 0;
            region.imageOffset.z = 0;
            region.imageExtent.width = width;
            region.imageExtent.height = height;
            region.imageExtent.depth = 1;
            transCmd.copyBufferToImage(tempBuffer, m_img, vk::ImageLayout::eTransferDstOptimal, region);

            vk::ImageMemoryBarrier use_barrier;
            use_barrier.sType = vk::StructureType::eImageMemoryBarrier;
            use_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            use_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            use_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            use_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            use_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            use_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            use_barrier.image = m_img;
            use_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            use_barrier.subresourceRange.levelCount = 1;
            use_barrier.subresourceRange.layerCount = 1;
            transCmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &use_barrier);
            transCmd.end();
            vk::SubmitInfo submitThis;
            submitThis.sType = vk::StructureType::eSubmitInfo;
            submitThis.commandBufferCount = 1;
            submitThis.pCommandBuffers = &transCmd;

            Core::Context::_graphicsQueue.submit(submitThis);
        }

        Core::Context::_graphicsQueue.waitIdle();

        Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, transCmd);

        Core::Context::_device->destroyBuffer(tempBuffer);
        Core::Context::_device->freeMemory(tempmemory);

		vk::DescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 0;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &samplerLayoutBinding;
		m_layout = Core::Context::_device->createDescriptorSetLayout(layoutInfo);

		vk::DescriptorSetAllocateInfo dinfo;
		dinfo.sType = vk::StructureType::eDescriptorSetAllocateInfo;
		dinfo.descriptorPool = Core::Context::_descriptorPool;
		dinfo.descriptorSetCount = 1;
		dinfo.pSetLayouts = &m_layout;

		m_set = Core::Context::_device->allocateDescriptorSets(dinfo).front();

		vk::DescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = m_view;
		imageInfo.sampler = m_sampler;

		vk::WriteDescriptorSet write;

		write.dstSet = m_set;
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfo;

		Core::Context::_device->updateDescriptorSets(write, nullptr);

	}
}
