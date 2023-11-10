#include "EnvironmentMap.hpp"
#include <HML/HeaderMath.hpp>
#include "Texture.hpp"
#include <fstream>
#include <iostream>

namespace Example
{
	namespace Utils
	{
		static void generateMips(vk::Image& img, uint32_t cubeMapSize);
		static void createTexture3D(vk::Image& img, vk::DeviceMemory& imgMemory, vk::ImageView& imgView, vk::Sampler& sampler, uint32_t cubeMapSize);
		static std::vector<char> readFile(std::string filename) {

			std::ifstream file(filename, std::ios::ate | std::ios::binary);
#ifdef M_DEBUG
			if (!file.is_open()) {
				std::cout << "Failed to load \"" << filename << "\"" << std::endl;
			}
#endif

			size_t filesize{ static_cast<size_t>(file.tellg()) };

			std::vector<char> buffer(filesize);
			file.seekg(0);
			file.read(buffer.data(), filesize);

			file.close();
			return buffer;
		}
		static vk::ShaderModule createShaderModule(std::string filename) {

			std::vector<char> sourceCode = readFile(filename);
			vk::ShaderModuleCreateInfo moduleInfo = {};
			moduleInfo.flags = vk::ShaderModuleCreateFlags();
			moduleInfo.codeSize = sourceCode.size();
			moduleInfo.pCode = reinterpret_cast<const uint32_t*>(sourceCode.data());
			return Core::Context::_device->createShaderModule(moduleInfo);
		}
	}

	void EnvironmentMap::Destroy()
	{
		Core::Context::_device->waitIdle();
		Core::Context::_device->destroyImageView(m_unfiltered.view);
		Core::Context::_device->destroySampler(m_unfiltered.sampler);
		Core::Context::_device->destroyImage(m_unfiltered.image);
		Core::Context::_device->freeMemory(m_unfiltered.memory);
	}
	void EnvironmentMap::load(const std::string& path)
	{
		constexpr uint32_t CubeMapSize = 1024;
		constexpr uint32_t IrradianceSize = 32;
		uint32_t IrradianceMapComputeSamples = 512;

		Utils::createTexture3D(m_unfiltered.image, m_unfiltered.memory, m_unfiltered.view, m_unfiltered.sampler, CubeMapSize);
		Utils::createTexture3D(m_radiance.image, m_radiance.memory, m_radiance.view, m_radiance.sampler, CubeMapSize);
		std::shared_ptr<Texture> equirectangular = std::make_shared<Texture>(path, false);		//Idk fix
		Utils::createTexture3D(m_irradiance.image, m_irradiance.memory, m_irradiance.view, m_irradiance.sampler, IrradianceSize);

		// Compute UnFilteredCubeMap Texture From Equirectangular Texture2D
		{
			vk::ShaderModule computeShader = Utils::createShaderModule("shaders/equirectangularToCube.spv");

			//Descriptors
			std::vector<vk::DescriptorSetLayoutBinding> bindings;
			vk::DescriptorSetLayoutBinding temp;
			temp.binding = 0;
			temp.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			temp.descriptorCount = 1;
			temp.stageFlags |= vk::ShaderStageFlagBits::eCompute;
			temp.pImmutableSamplers = nullptr;
			bindings.push_back(temp);
			temp.binding = 1;
			temp.descriptorType = vk::DescriptorType::eStorageImage;
			temp.descriptorCount = 1;
			temp.stageFlags |= vk::ShaderStageFlagBits::eCompute;
			temp.pImmutableSamplers = nullptr;
			bindings.push_back(temp);

			vk::DescriptorSetLayoutCreateInfo layoutInfo;
			layoutInfo.sType = vk::StructureType::eDescriptorSetLayoutCreateInfo;
			layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
			layoutInfo.pBindings = bindings.data();

			vk::DescriptorSetLayout descriptorLayout = Core::Context::_device->createDescriptorSetLayout(layoutInfo);

			vk::DescriptorSetAllocateInfo allocateInfo;
			allocateInfo.sType = vk::StructureType::eDescriptorSetAllocateInfo;
			allocateInfo.descriptorPool = Core::Context::_descriptorPool;
			allocateInfo.descriptorSetCount = 1;
			allocateInfo.pSetLayouts = &descriptorLayout;

			vk::DescriptorSet set = Core::Context::_device->allocateDescriptorSets(allocateInfo).front();

			vk::DescriptorImageInfo desc_image0;
			vk::DescriptorImageInfo desc_image1;
			vk::WriteDescriptorSet write;

			desc_image0.sampler = equirectangular->GetSampler();
			desc_image0.imageView = equirectangular->GetImageView();
			desc_image0.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;		//IDK

			write.sType = vk::StructureType::eWriteDescriptorSet;
			write.dstSet = set;
			write.dstBinding = 0;
			write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			write.descriptorCount = 1;
			write.pImageInfo = &desc_image0;

			Core::Context::_device->updateDescriptorSets(write, nullptr);

			desc_image1.sampler = m_unfiltered.sampler;
			desc_image1.imageView = m_unfiltered.view;
			desc_image1.imageLayout = vk::ImageLayout::eGeneral;		//IDK

			write.sType = vk::StructureType::eWriteDescriptorSet;
			write.dstSet = set;
			write.dstBinding = 1;
			write.descriptorType = vk::DescriptorType::eStorageImage;
			write.descriptorCount = 1;
			write.pImageInfo = &desc_image1;

			Core::Context::_device->updateDescriptorSets(write, nullptr);

			vk::PipelineShaderStageCreateInfo computeShaderInfo{};
			computeShaderInfo.flags = vk::PipelineShaderStageCreateFlags();
			computeShaderInfo.stage = vk::ShaderStageFlagBits::eCompute;
			computeShaderInfo.module = computeShader;
			computeShaderInfo.pName = "main";

			vk::PipelineLayoutCreateInfo layoutCreateInfo;
			layoutCreateInfo.sType = vk::StructureType::ePipelineLayoutCreateInfo;
			layoutCreateInfo.setLayoutCount = 1;
			layoutCreateInfo.pSetLayouts = &descriptorLayout;
			layoutCreateInfo.pushConstantRangeCount = 0;
			layoutCreateInfo.pPushConstantRanges = nullptr;

			vk::PipelineLayout pipelineLayout = Core::Context::_device->createPipelineLayout(layoutCreateInfo);

			vk::ComputePipelineCreateInfo compPipelineInfo;
			compPipelineInfo.sType = vk::StructureType::eComputePipelineCreateInfo;
			compPipelineInfo.stage = computeShaderInfo;
			compPipelineInfo.layout = pipelineLayout;
			compPipelineInfo.basePipelineHandle = nullptr;
			compPipelineInfo.basePipelineIndex = -1;

			vk::Pipeline pipeline = Core::Context::_device->createComputePipeline(VK_NULL_HANDLE, compPipelineInfo).value;

			vk::CommandBufferAllocateInfo cmdAllocateInfo;
			cmdAllocateInfo.sType = vk::StructureType::eCommandBufferAllocateInfo;
			cmdAllocateInfo.commandPool = Core::Context::_graphicsCommandPool;
			cmdAllocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
			cmdAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
			vk::CommandBuffer cmd = Core::Context::_device->allocateCommandBuffers(cmdAllocateInfo).front();
			vk::CommandBufferBeginInfo begininfo;
			begininfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
			cmd.begin(begininfo);
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, { set }, {});
			cmd.dispatch(CubeMapSize / 32, CubeMapSize / 32, 6);
			cmd.end();
			vk::SubmitInfo submitInfo;
			submitInfo.sType = vk::StructureType::eSubmitInfo;
			submitInfo.waitSemaphoreCount = 0;
			submitInfo.pWaitSemaphores = nullptr;
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmd;
			Core::Context::_graphicsQueue.submit(submitInfo);
			Core::Context::_device->waitIdle();
			Utils::generateMips(m_unfiltered.image, CubeMapSize);

			equirectangular->Destroy();
			Core::Context::_device->destroyShaderModule(computeShader);
			Core::Context::_device->destroyPipeline(pipeline);
			Core::Context::_device->destroyPipelineLayout(pipelineLayout);
			Core::Context::_device->freeDescriptorSets(Core::Context::_descriptorPool, set);
			Core::Context::_device->destroyDescriptorSetLayout(descriptorLayout);
			Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, cmd);
		}
		// Compute RadienceMap Texture From UnfilteredCubeMap Texture
		{
			uint32_t mipCount = (uint32_t)std::floor(std::log2(HML::Min(CubeMapSize, CubeMapSize))) + 1;

			vk::ShaderModule computeShader = Utils::createShaderModule("shaders/envCalculateRadianceMap.spv");

			//Descriptors
			std::vector<vk::DescriptorSetLayoutBinding> bindings;
			vk::DescriptorSetLayoutBinding temp;
			temp.binding = 0;
			temp.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			temp.descriptorCount = 1;
			temp.stageFlags |= vk::ShaderStageFlagBits::eCompute;
			temp.pImmutableSamplers = nullptr;
			bindings.push_back(temp);
			temp.binding = 1;
			temp.descriptorType = vk::DescriptorType::eStorageImage;
			temp.descriptorCount = 1;
			temp.stageFlags |= vk::ShaderStageFlagBits::eCompute;
			temp.pImmutableSamplers = nullptr;
			bindings.push_back(temp);

			vk::DescriptorSetLayoutCreateInfo layoutInfo;
			layoutInfo.sType = vk::StructureType::eDescriptorSetLayoutCreateInfo;
			layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
			layoutInfo.pBindings = bindings.data();

			vk::DescriptorSetLayout descriptorLayout = Core::Context::_device->createDescriptorSetLayout(layoutInfo);
			vk::DescriptorSetAllocateInfo allocateInfo;
			allocateInfo.sType = vk::StructureType::eDescriptorSetAllocateInfo;
			allocateInfo.descriptorPool = Core::Context::_descriptorPool;
			allocateInfo.descriptorSetCount = 1;
			allocateInfo.pSetLayouts = &descriptorLayout;


			std::vector<vk::DescriptorSet> sets;
			std::vector<vk::ImageView> views;
			for (uint32_t i = 0; i < mipCount; i++)
			{
				vk::ImageView tempView = views.emplace_back();
				vk::ImageViewCreateInfo imgViewInfo;
				imgViewInfo.sType = vk::StructureType::eImageViewCreateInfo;
				imgViewInfo.viewType = vk::ImageViewType::eCube;
				imgViewInfo.format = vk::Format::eR32G32B32A32Sfloat;
				imgViewInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
				imgViewInfo.subresourceRange = vk::ImageSubresourceRange({});
				imgViewInfo.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eColor;
				imgViewInfo.subresourceRange.baseMipLevel = i;
				imgViewInfo.subresourceRange.levelCount = 1;
				imgViewInfo.subresourceRange.baseArrayLayer = 0;
				imgViewInfo.subresourceRange.layerCount = 6;
				imgViewInfo.image = m_radiance.image;

				tempView = Core::Context::_device->createImageView(imgViewInfo);
				vk::DescriptorSet set = Core::Context::_device->allocateDescriptorSets(allocateInfo).front();
				vk::DescriptorImageInfo desc_image0;
				vk::DescriptorImageInfo desc_image1;
				vk::WriteDescriptorSet write;

				desc_image0.sampler = m_unfiltered.sampler;
				desc_image0.imageView = m_unfiltered.view;
				desc_image0.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;		//IDK

				write.sType = vk::StructureType::eWriteDescriptorSet;
				write.dstSet = set;
				write.dstBinding = 0;
				write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
				write.descriptorCount = 1;
				write.pImageInfo = &desc_image0;

				Core::Context::_device->updateDescriptorSets(write, nullptr);

				desc_image1.sampler = m_radiance.sampler;
				desc_image1.imageView = tempView;
				desc_image1.imageLayout = vk::ImageLayout::eGeneral;		//IDK

				write.sType = vk::StructureType::eWriteDescriptorSet;
				write.dstSet = set;
				write.dstBinding = 1;
				write.descriptorType = vk::DescriptorType::eStorageImage;
				write.descriptorCount = 1;
				write.pImageInfo = &desc_image1;

				Core::Context::_device->updateDescriptorSets(write, nullptr);
				sets.push_back(set);
			}
			vk::PipelineShaderStageCreateInfo computeShaderInfo{};
			computeShaderInfo.flags = vk::PipelineShaderStageCreateFlags();
			computeShaderInfo.stage = vk::ShaderStageFlagBits::eCompute;
			computeShaderInfo.module = computeShader;
			computeShaderInfo.pName = "main";

			vk::PushConstantRange pushC;
			pushC.offset = 0;
			pushC.size = sizeof(float);
			pushC.stageFlags = vk::ShaderStageFlagBits::eCompute;

			vk::PipelineLayoutCreateInfo layoutCreateInfo;
			layoutCreateInfo.sType = vk::StructureType::ePipelineLayoutCreateInfo;
			layoutCreateInfo.setLayoutCount = 1;
			layoutCreateInfo.pSetLayouts = &descriptorLayout;
			layoutCreateInfo.pushConstantRangeCount = 1;
			layoutCreateInfo.pPushConstantRanges = &pushC;

			vk::PipelineLayout pipelineLayout = Core::Context::_device->createPipelineLayout(layoutCreateInfo);

			vk::ComputePipelineCreateInfo compPipelineInfo;
			compPipelineInfo.sType = vk::StructureType::eComputePipelineCreateInfo;
			compPipelineInfo.stage = computeShaderInfo;
			compPipelineInfo.layout = pipelineLayout;
			compPipelineInfo.basePipelineHandle = nullptr;
			compPipelineInfo.basePipelineIndex = -1;

			vk::Pipeline pipeline = Core::Context::_device->createComputePipeline(VK_NULL_HANDLE, compPipelineInfo).value;

			vk::CommandBufferAllocateInfo cmdAllocateInfo;
			cmdAllocateInfo.sType = vk::StructureType::eCommandBufferAllocateInfo;
			cmdAllocateInfo.commandPool = Core::Context::_graphicsCommandPool;
			cmdAllocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
			cmdAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
			vk::CommandBuffer cmd = Core::Context::_device->allocateCommandBuffers(cmdAllocateInfo).front();
			vk::CommandBufferBeginInfo begininfo;
			begininfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
			cmd.begin(begininfo);
			const float deltaRoughness = 1.0f / HML::Max((float)mipCount - 1.0f, 1.0f);
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
			for (uint32_t i = 0, size = CubeMapSize; i < mipCount; i++, size /= 2)
			{
				uint32_t numGroups = HML::Max(1u, size / 32);
				float roughness = i * deltaRoughness;
				roughness = HML::Max(roughness, 0.05f);
				cmd.pushConstants(pipelineLayout , vk::ShaderStageFlagBits::eCompute, 0, sizeof(float), &roughness);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, { sets[i] }, {});
				cmd.dispatch(numGroups, numGroups, 6);
			}
			cmd.end();
			vk::SubmitInfo submitInfo;
			submitInfo.sType = vk::StructureType::eSubmitInfo;
			submitInfo.waitSemaphoreCount = 0;
			submitInfo.pWaitSemaphores = nullptr;
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmd;
			Core::Context::_graphicsQueue.submit(submitInfo);
			Core::Context::_device->waitIdle();
			Utils::generateMips(m_unfiltered.image, CubeMapSize);
			Core::Context::_device->destroyShaderModule(computeShader);
			Core::Context::_device->destroyPipeline(pipeline);
			Core::Context::_device->destroyPipelineLayout(pipelineLayout);
			for (auto set : sets)
				Core::Context::_device->freeDescriptorSets(Core::Context::_descriptorPool, set);
			sets.clear();
			Core::Context::_device->destroyDescriptorSetLayout(descriptorLayout);
			Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, cmd);
		}
		// Compute IrradianceMap Texture From RadienceMap Texture
		{
			uint32_t mipCount = (uint32_t)std::floor(std::log2(HML::Min(CubeMapSize, CubeMapSize))) + 1;

			vk::ShaderModule computeShader = Utils::createShaderModule("shaders/envCalculateIrradianceMap.spv");

			//Descriptors
			std::vector<vk::DescriptorSetLayoutBinding> bindings;
			vk::DescriptorSetLayoutBinding temp;
			temp.binding = 0;
			temp.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			temp.descriptorCount = 1;
			temp.stageFlags = vk::ShaderStageFlagBits::eCompute;
			temp.pImmutableSamplers = nullptr;
			bindings.push_back(temp);
			temp.binding = 1;
			temp.descriptorType = vk::DescriptorType::eStorageImage;
			temp.descriptorCount = 1;
			temp.stageFlags = vk::ShaderStageFlagBits::eCompute;
			temp.pImmutableSamplers = nullptr;
			bindings.push_back(temp);

			vk::DescriptorSetLayoutCreateInfo layoutInfo;
			layoutInfo.sType = vk::StructureType::eDescriptorSetLayoutCreateInfo;
			layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
			layoutInfo.pBindings = bindings.data();

			vk::DescriptorSetLayout descriptorLayout = Core::Context::_device->createDescriptorSetLayout(layoutInfo);
			vk::DescriptorSetAllocateInfo allocateInfo;
			allocateInfo.sType = vk::StructureType::eDescriptorSetAllocateInfo;
			allocateInfo.descriptorPool = Core::Context::_descriptorPool;
			allocateInfo.descriptorSetCount = 1;
			allocateInfo.pSetLayouts = &descriptorLayout;

			vk::DescriptorSet set = Core::Context::_device->allocateDescriptorSets(allocateInfo).front();

			vk::DescriptorImageInfo desc_image0;
			vk::WriteDescriptorSet write;

			desc_image0.sampler = m_radiance.sampler;
			desc_image0.imageView = m_radiance.view;
			desc_image0.imageLayout = vk::ImageLayout::eGeneral;		//IDK

			write.sType = vk::StructureType::eWriteDescriptorSet;
			write.dstSet = set;
			write.dstBinding = 0;
			write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			write.descriptorCount = 1;
			write.pImageInfo = &desc_image0;

			Core::Context::_device->updateDescriptorSets(write, nullptr);

			vk::DescriptorImageInfo desc_image1;

			desc_image1.sampler = m_irradiance.sampler;
			desc_image1.imageView = m_irradiance.view;
			desc_image1.imageLayout = vk::ImageLayout::eGeneral;		//IDK

			write.sType = vk::StructureType::eWriteDescriptorSet;
			write.dstSet = set;
			write.dstBinding = 1;
			write.descriptorType = vk::DescriptorType::eStorageImage;
			write.descriptorCount = 1;
			write.pImageInfo = &desc_image1;

			Core::Context::_device->updateDescriptorSets(write, nullptr);

			vk::PipelineShaderStageCreateInfo computeShaderInfo{};
			computeShaderInfo.flags = vk::PipelineShaderStageCreateFlags();
			computeShaderInfo.stage = vk::ShaderStageFlagBits::eCompute;
			computeShaderInfo.module = computeShader;
			computeShaderInfo.pName = "main";

			vk::PushConstantRange pushC;
			pushC.offset = 0;
			pushC.size = sizeof(uint32_t);
			pushC.stageFlags = vk::ShaderStageFlagBits::eCompute;

			vk::PipelineLayoutCreateInfo layoutCreateInfo;
			layoutCreateInfo.sType = vk::StructureType::ePipelineLayoutCreateInfo;
			layoutCreateInfo.setLayoutCount = 1;
			layoutCreateInfo.pSetLayouts = &descriptorLayout;
			layoutCreateInfo.pushConstantRangeCount = 1;
			layoutCreateInfo.pPushConstantRanges = &pushC;

			vk::PipelineLayout pipelineLayout = Core::Context::_device->createPipelineLayout(layoutCreateInfo);

			vk::ComputePipelineCreateInfo compPipelineInfo;
			compPipelineInfo.sType = vk::StructureType::eComputePipelineCreateInfo;
			compPipelineInfo.stage = computeShaderInfo;
			compPipelineInfo.layout = pipelineLayout;
			compPipelineInfo.basePipelineHandle = nullptr;
			compPipelineInfo.basePipelineIndex = -1;

			vk::Pipeline pipeline = Core::Context::_device->createComputePipeline(VK_NULL_HANDLE, compPipelineInfo).value;

			vk::CommandBufferAllocateInfo cmdAllocateInfo;
			cmdAllocateInfo.sType = vk::StructureType::eCommandBufferAllocateInfo;
			cmdAllocateInfo.commandPool = Core::Context::_graphicsCommandPool;
			cmdAllocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
			cmdAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
			vk::CommandBuffer cmd = Core::Context::_device->allocateCommandBuffers(cmdAllocateInfo).front();
			vk::CommandBufferBeginInfo begininfo;
			begininfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
			cmd.begin(begininfo);
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
			cmd.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint32_t), &IrradianceMapComputeSamples);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, { set }, {});
			cmd.dispatch(IrradianceSize / 32, IrradianceSize / 32, 6);
			cmd.end();
			vk::SubmitInfo submitInfo;
			submitInfo.sType = vk::StructureType::eSubmitInfo;
			submitInfo.waitSemaphoreCount = 0;
			submitInfo.pWaitSemaphores = nullptr;
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmd;
			Core::Context::_graphicsQueue.submit(submitInfo);
			Core::Context::_device->waitIdle();
			Utils::generateMips(m_unfiltered.image, CubeMapSize);
			Core::Context::_device->destroyShaderModule(computeShader);
			Core::Context::_device->destroyPipeline(pipeline);
			Core::Context::_device->destroyPipelineLayout(pipelineLayout);
			Core::Context::_device->freeDescriptorSets(Core::Context::_descriptorPool, set);
			Core::Context::_device->destroyDescriptorSetLayout(descriptorLayout);
			Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, cmd);
		}
	}
	namespace Utils
	{
		static void generateMips(vk::Image& img, uint32_t cubeMapSize)
		{
			uint32_t mipLevels = (uint32_t)std::floor(std::log2(HML::Min(cubeMapSize, cubeMapSize))) + 1;
			//Generate Mips
			{
				vk::CommandBufferAllocateInfo allocinfo;
				allocinfo.sType = vk::StructureType::eCommandBufferAllocateInfo;
				allocinfo.level = vk::CommandBufferLevel::ePrimary;
				allocinfo.commandPool = Core::Context::_graphicsCommandPool;
				allocinfo.commandBufferCount = 1;
				auto tempCmd = Core::Context::_device->allocateCommandBuffers(allocinfo).front();
				vk::CommandBufferBeginInfo beg;
				beg.sType = vk::StructureType::eCommandBufferBeginInfo;
				beg.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
				tempCmd.begin(beg);
				for (uint32_t face = 0; face < 6; face++)
				{
					vk::ImageMemoryBarrier barrier;
					barrier.srcAccessMask = vk::AccessFlags(0);
					barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
					barrier.oldLayout = vk::ImageLayout::eGeneral;
					barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
					barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.image = img;
					barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
					barrier.subresourceRange.baseMipLevel = 0;
					barrier.subresourceRange.levelCount = 1;
					barrier.subresourceRange.baseArrayLayer = face;
					barrier.subresourceRange.layerCount = 1;
					tempCmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, barrier);
				}
				for (uint32_t level = 1; level < mipLevels; level++)
				{
					for (uint32_t face = 0; face < 6; face++)
					{
						vk::ImageMemoryBarrier preBlit;
						preBlit.srcAccessMask = vk::AccessFlags(0);
						preBlit.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
						preBlit.oldLayout = vk::ImageLayout::eUndefined;
						preBlit.newLayout = vk::ImageLayout::eTransferDstOptimal;
						preBlit.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						preBlit.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						preBlit.image = img;
						preBlit.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
						preBlit.subresourceRange.baseMipLevel = level;
						preBlit.subresourceRange.baseArrayLayer = face;
						preBlit.subresourceRange.levelCount = 1;
						preBlit.subresourceRange.layerCount = 1;

						tempCmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, preBlit);

						vk::ImageBlit region = {};
						region.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
						region.srcSubresource.layerCount = 1;
						region.srcSubresource.mipLevel = level - 1;
						region.srcSubresource.baseArrayLayer = face;
						region.srcOffsets[1].x = int32_t(cubeMapSize >> (level - 1));
						region.srcOffsets[1].y = int32_t(cubeMapSize >> (level - 1));
						region.srcOffsets[1].z = int32_t(1);
						region.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
						region.dstSubresource.layerCount = 1;
						region.dstSubresource.mipLevel = level;
						region.dstSubresource.baseArrayLayer = face;
						region.dstOffsets[1].x = int32_t(cubeMapSize >> level);
						region.dstOffsets[1].y = int32_t(cubeMapSize >> level);
						region.dstOffsets[1].z = int32_t(1);

						tempCmd.blitImage(img, vk::ImageLayout::eTransferSrcOptimal, img, vk::ImageLayout::eTransferDstOptimal, region, vk::Filter::eLinear);

						vk::ImageMemoryBarrier postBlit;
						postBlit.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
						postBlit.dstAccessMask = vk::AccessFlagBits::eTransferRead;
						postBlit.oldLayout = vk::ImageLayout::eTransferDstOptimal;
						postBlit.newLayout = vk::ImageLayout::eTransferSrcOptimal;
						postBlit.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						postBlit.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						postBlit.image = img;
						postBlit.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
						postBlit.subresourceRange.baseMipLevel = level;
						postBlit.subresourceRange.baseArrayLayer = face;
						postBlit.subresourceRange.levelCount = 1;
						postBlit.subresourceRange.layerCount = 1;

						tempCmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, postBlit);
					}
				}
				// Transition whole mip chain to shader read only layout.
				{
					vk::ImageMemoryBarrier barrier;
					barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
					barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
					barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
					barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;        //TODO:(Jacob) add a switch for this if necessary for feature post processing effects
					barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.image = img;
					barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
					barrier.subresourceRange.levelCount = mipLevels;
					barrier.subresourceRange.layerCount = 6;
					tempCmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), nullptr, nullptr, barrier);
				}

				tempCmd.end();
				vk::SubmitInfo submitThis;
				submitThis.sType = vk::StructureType::eSubmitInfo;
				submitThis.commandBufferCount = 1;
				submitThis.pCommandBuffers = &tempCmd;
				Core::Context::_graphicsQueue.submit(submitThis);
				Core::Context::_graphicsQueue.waitIdle();
				Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, tempCmd);
			}
		}
		static void createTexture3D(vk::Image& img, vk::DeviceMemory& imgMemory, vk::ImageView& imgView, vk::Sampler& sampler, uint32_t cubeMapSize)
		{
			uint32_t channelCount = 4;
			uint32_t faceCount = 6;
			uint32_t size = cubeMapSize * cubeMapSize * channelCount * faceCount;
			uint32_t mipLevels = (uint32_t)std::floor(std::log2(HML::Min(cubeMapSize, cubeMapSize))) + 1;
			vk::Format format = vk::Format::eR32G32B32A32Sfloat;

			//Create Image
			{
				vk::ImageCreateInfo imgInfo;
				imgInfo.sType = vk::StructureType::eImageCreateInfo;
				imgInfo.imageType = vk::ImageType::e2D;
				imgInfo.extent.width = cubeMapSize;
				imgInfo.extent.height = cubeMapSize;
				imgInfo.extent.depth = 1;
				imgInfo.mipLevels = mipLevels;
				imgInfo.arrayLayers = 6;
				imgInfo.format = format;
				imgInfo.tiling = vk::ImageTiling::eOptimal;
				imgInfo.initialLayout = vk::ImageLayout::eUndefined;
				imgInfo.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
				imgInfo.samples = vk::SampleCountFlagBits::e1;
				imgInfo.sharingMode = vk::SharingMode::eExclusive;
				imgInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;

				img = Core::Context::_device->createImage(imgInfo);

				vk::MemoryRequirements memReqs = Core::Context::_device->getImageMemoryRequirements(img);
				vk::MemoryAllocateInfo memAlloc;
				memAlloc.sType = vk::StructureType::eMemoryAllocateInfo;
				memAlloc.allocationSize = memReqs.size;
				memAlloc.memoryTypeIndex = Core::Context::findMemoryType(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
				imgMemory = Core::Context::_device->allocateMemory(memAlloc);
				Core::Context::_device->bindImageMemory(img, imgMemory, /*vk::DeviceSize*/0);
			}
			vk::CommandBufferAllocateInfo allocinfo;
			allocinfo.sType = vk::StructureType::eCommandBufferAllocateInfo;
			allocinfo.level = vk::CommandBufferLevel::ePrimary;
			allocinfo.commandPool = Core::Context::_graphicsCommandPool;
			allocinfo.commandBufferCount = 1;
			auto tempCmd = Core::Context::_device->allocateCommandBuffers(allocinfo).front();
			vk::CommandBufferBeginInfo beg;
			beg.sType = vk::StructureType::eCommandBufferBeginInfo;
			beg.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
			tempCmd.begin(beg);

			vk::ImageMemoryBarrier imgBarrier = {};
			imgBarrier.sType = vk::StructureType::eImageMemoryBarrier;
			imgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imgBarrier.oldLayout = vk::ImageLayout::eUndefined;
			imgBarrier.newLayout = vk::ImageLayout::eGeneral;
			imgBarrier.image = img;
			imgBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			imgBarrier.subresourceRange.baseMipLevel = 0;
			imgBarrier.subresourceRange.levelCount = mipLevels;
			imgBarrier.subresourceRange.layerCount = 6;
			tempCmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, imgBarrier);

			tempCmd.end();
			vk::SubmitInfo submitThis;
			submitThis.sType = vk::StructureType::eSubmitInfo;
			submitThis.commandBufferCount = 1;
			submitThis.pCommandBuffers = &tempCmd;
			Core::Context::_graphicsQueue.submit(submitThis);
			Core::Context::_graphicsQueue.waitIdle();
			Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, tempCmd);
			//Create Image View
			{
				vk::ImageViewCreateInfo imgViewInfo;
				imgViewInfo.sType = vk::StructureType::eImageViewCreateInfo;
				imgViewInfo.viewType = vk::ImageViewType::eCube;
				imgViewInfo.format = format;
				imgViewInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
				imgViewInfo.subresourceRange = vk::ImageSubresourceRange({});
				imgViewInfo.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eColor;
				imgViewInfo.subresourceRange.baseMipLevel = 0;
				imgViewInfo.subresourceRange.levelCount = mipLevels;
				imgViewInfo.subresourceRange.baseArrayLayer = 0;
				imgViewInfo.subresourceRange.layerCount = 6;
				imgViewInfo.image = img;
				imgView = Core::Context::_device->createImageView(imgViewInfo);
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
				samplerInfo.anisotropyEnable = false;
				samplerInfo.mipLodBias = 0.0f;
				samplerInfo.maxAnisotropy = 1.0f;
				samplerInfo.minLod = 0.0f;
				samplerInfo.maxLod = mipLevels;
				samplerInfo.compareOp = vk::CompareOp::eNever;
				samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
				sampler = Core::Context::_device->createSampler(samplerInfo);
			}
			//Set Data (Delete If unnecessary)
			{

			}
			generateMips(img, cubeMapSize);
		}
	}
}