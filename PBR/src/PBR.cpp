#include "PBR.hpp"
#include <HML/HeaderMath.hpp>
#include "API/VulkanInit.hpp"
#include "App.hpp"
#include "API/RenderPass.h"
#include "API/FrameBuffer.h"
#include "API/GraphicsPipeline.h"
#include <string>
#include <fstream>
#include "Model.hpp"
#include "Camera.hpp"
#include "Texture.hpp"
#include "EnvironmentMap.hpp"

namespace Example
{
	static uint32_t findMemoryTypeIndex(uint32_t allowedTypes, vk::MemoryPropertyFlags flags)
	{
		auto props = Core::Context::_GPU.getMemoryProperties();

		for (uint32_t i = 0; i < props.memoryTypeCount; i++)
		{
			if ((allowedTypes & (1 << i))
				&& (props.memoryTypes[i].propertyFlags & flags) == flags)
			{
				return i;
			}
		}
		return -1;
	}

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

	template <class integral>
	constexpr bool is_aligned(integral x, size_t a) noexcept
	{
		return (x & (integral(a) - 1)) == 0;
	}

	template <class integral>
	constexpr integral align_up(integral x, size_t a) noexcept
	{
		return integral((x + (integral(a) - 1)) & ~integral(a - 1));
	}

	template <class integral>
	constexpr integral align_down(integral x, size_t a) noexcept
	{
		return integral(x & ~integral(a - 1));
	}

	static vk::ShaderModule createShaderModule(std::string filename) {

		std::vector<char> sourceCode = readFile(filename);
		vk::ShaderModuleCreateInfo moduleInfo = {};
		moduleInfo.flags = vk::ShaderModuleCreateFlags();
		moduleInfo.codeSize = sourceCode.size();
		moduleInfo.pCode = reinterpret_cast<const uint32_t*>(sourceCode.data());
		return Core::Context::_device->createShaderModule(moduleInfo);
	}
	struct Data
	{
		struct Constant
		{
			HML::Mat4x4<> ModelTransform;
		};
		HML::Mat4x4<> ShadowTransform = HML::Mat4x4<>(1.0f);

		std::shared_ptr<Core::RenderPass> geometryRenderPass;
		std::shared_ptr<Core::RenderPass> compositeRenderPass;
		std::shared_ptr<Core::FrameBuffer> offScreenFrameBuffer;
		std::shared_ptr<Core::FrameBuffer> swapchainTarget;
		std::shared_ptr<Core::GraphicsPipeline> geometryPipeline;
		std::shared_ptr<Core::GraphicsPipeline> skyboxPipeline;
		std::shared_ptr<Core::GraphicsPipeline> compositePipeline;
		std::vector<vk::CommandBuffer> cmd;
		//World Data
		HML::Vector3<> Position = {0.0f, 0.0f, 5.0f};
		Constant pushConstant;
		float rotation = 1.5707963268f;
		std::shared_ptr<Model> axe;
		std::shared_ptr<Texture> brdfLut;
		std::shared_ptr<EnvironmentMap> environmentMap;
		std::shared_ptr<Camera> cam;
		vk::DescriptorSetLayout CamDescriptorSetLayout;
		vk::DescriptorSet CamDescriptorSet;
		vk::DescriptorSetLayout CompositeDescriptorSetLayout;
		vk::DescriptorSet CompositeDescriptorSet;

		vk::Buffer quadVertexBuffer;
		vk::DeviceMemory quadVertexBufferMem;
		vk::DeviceSize quadVertexBufferSize = (4 * sizeof(HML::Vector3<>)) + (4 * sizeof(HML::Vector2<>));

		vk::Buffer quadIndexBuffer;
		vk::DeviceMemory quadIndexBufferMem;
		vk::DeviceSize quadIndexBufferSize = 6 * sizeof(uint32_t);

		vk::DescriptorSetLayout RT_SetLayout;
		vk::DescriptorSet Rt_Set;
		struct StorageImage
		{
			vk::Image handle;
			vk::ImageView view_handle;
			vk::DeviceMemory mem;
		};
		struct ShaderBindingTableBufferData
		{
			vk::Buffer buffer;
			vk::DeviceMemory mem;
			vk::DeviceSize size;
		};
		ShaderBindingTableBufferData RTShaderBindingTableBuffer;
		StorageImage storageImg;
		std::vector<vk::RayTracingShaderGroupCreateInfoKHR> rtShaderGroups;

		bool needsResize = true;
	};
	static Data* s_Data = new Data();

	static void CreateDescriptorSets()
	{

	}

	static void CreateCameraUniformBuffer()
	{

	}

	static void Create()
	{
		//SkyVertexBuffer
		{
			vk::BufferCreateInfo bufferInfo;
			bufferInfo.sType = vk::StructureType::eBufferCreateInfo;
			bufferInfo.size = s_Data->quadVertexBufferSize;
			bufferInfo.usage |= vk::BufferUsageFlagBits::eVertexBuffer;
			bufferInfo.sharingMode = vk::SharingMode::eExclusive;
			s_Data->quadVertexBuffer = Core::Context::_device->createBuffer(bufferInfo);
			auto memReq = Core::Context::_device->getBufferMemoryRequirements(s_Data->quadVertexBuffer);

			vk::MemoryAllocateInfo allocate;
			allocate.sType = vk::StructureType::eMemoryAllocateInfo;
			allocate.allocationSize = memReq.size;
			vk::MemoryPropertyFlags flags;

			//flags |= vk::MemoryPropertyFlagBits::eDeviceLocal;
			flags |= vk::MemoryPropertyFlagBits::eHostVisible;
			flags |= vk::MemoryPropertyFlagBits::eHostCoherent;

			allocate.memoryTypeIndex = findMemoryTypeIndex(memReq.memoryTypeBits, flags);

			s_Data->quadVertexBufferMem = Core::Context::_device->allocateMemory(allocate);
			Core::Context::_device->bindBufferMemory(s_Data->quadVertexBuffer, s_Data->quadVertexBufferMem, 0);

			struct FullScreenQuadVertex
			{
				HML::Vector3<> Position;
				HML::Vector2<> Uv;
			};

			FullScreenQuadVertex* data = new FullScreenQuadVertex[4];
			float x = -1;
			float y = -1;
			float width = 2, height = 2;
			data[0].Position = HML::Vector3<>(x, y, 0.0f);
			data[0].Uv = HML::Vector2<>(0, 0);

			data[1].Position = HML::Vector3<>(x + width, y, 0.0f);
			data[1].Uv = HML::Vector2<>(1, 0);

			data[2].Position = HML::Vector3<>(x + width, y + height, 0.0f);
			data[2].Uv = HML::Vector2<>(1, 1);

			data[3].Position = HML::Vector3<>(x, y + height, 0.0f);
			data[3].Uv = HML::Vector2<>(0, 1);

			auto m_data = Core::Context::_device->mapMemory(s_Data->quadVertexBufferMem, 0, s_Data->quadVertexBufferSize);     //FIX
			memcpy(m_data, data, s_Data->quadVertexBufferSize);
			Core::Context::_device->unmapMemory(s_Data->quadVertexBufferMem);

			delete[] data;
		}
		//SkyIndexBuffer
		{
			vk::BufferCreateInfo bufferInfo;
			bufferInfo.sType = vk::StructureType::eBufferCreateInfo;
			bufferInfo.size = s_Data->quadIndexBufferSize;
			bufferInfo.usage |= vk::BufferUsageFlagBits::eIndexBuffer;
			bufferInfo.sharingMode = vk::SharingMode::eExclusive;
			s_Data->quadIndexBuffer = Core::Context::_device->createBuffer(bufferInfo);
			auto memReq = Core::Context::_device->getBufferMemoryRequirements(s_Data->quadIndexBuffer);

			vk::MemoryAllocateInfo allocate;
			allocate.sType = vk::StructureType::eMemoryAllocateInfo;
			allocate.allocationSize = memReq.size;
			vk::MemoryPropertyFlags flags;

			//flags |= vk::MemoryPropertyFlagBits::eDeviceLocal;
			flags |= vk::MemoryPropertyFlagBits::eHostVisible;
			flags |= vk::MemoryPropertyFlagBits::eHostCoherent;

			allocate.memoryTypeIndex = findMemoryTypeIndex(memReq.memoryTypeBits, flags);

			s_Data->quadIndexBufferMem = Core::Context::_device->allocateMemory(allocate);
			Core::Context::_device->bindBufferMemory(s_Data->quadIndexBuffer, s_Data->quadIndexBufferMem, 0);

			uint32_t quadIndices[6] = { 0, 1, 2, 2, 3, 0 };

			auto m_data = Core::Context::_device->mapMemory(s_Data->quadIndexBufferMem, 0, s_Data->quadIndexBufferSize);     //FIX
			memcpy(m_data, &quadIndices, s_Data->quadIndexBufferSize);
			Core::Context::_device->unmapMemory(s_Data->quadIndexBufferMem);
		}

		//Renderpass
#ifdef M_DEBUG
		std::cout << "Create RenderPass" << std::endl;
#endif
		s_Data->geometryRenderPass = std::make_shared<Core::RenderPass>(true);		//Maybe Depth attachment alse false ?
		s_Data->compositeRenderPass = std::make_shared<Core::RenderPass>(false);		//Maybe Depth attachment alse false ?
		//Graphics Pipeline
		{
			vk::VertexInputBindingDescription bdesc;
			std::vector<vk::VertexInputAttributeDescription> attributes;

			bdesc.binding = 0;
			bdesc.inputRate = vk::VertexInputRate::eVertex;
			bdesc.stride = sizeof(Vertex);

			vk::VertexInputAttributeDescription attr;
			attr.binding = 0;
			attr.location = 0;
			attr.format = vk::Format::eR32G32B32Sfloat;
			attr.offset = offsetof(Vertex, Vertex::position);
			attributes.push_back(attr);

			attr.binding = 0;
			attr.location = 1;
			attr.format = vk::Format::eR32G32B32Sfloat;
			attr.offset = offsetof(Vertex, Vertex::normal);
			attributes.push_back(attr);

			attr.binding = 0;
			attr.location = 2;
			attr.format = vk::Format::eR32G32B32A32Sfloat;
			attr.offset = offsetof(Vertex, Vertex::tangent);
			attributes.push_back(attr);

			attr.binding = 0;
			attr.location = 3;
			attr.format = vk::Format::eR32G32B32A32Sfloat;
			attr.offset = offsetof(Vertex, Vertex::bitangent);
			attributes.push_back(attr);

			attr.binding = 0;
			attr.location = 4;
			attr.format = vk::Format::eR32G32B32A32Sfloat;
			attr.offset = offsetof(Vertex, Vertex::color);
			attributes.push_back(attr);

			attr.binding = 0;
			attr.location = 5;
			attr.format = vk::Format::eR32G32Sfloat;
			attr.offset = offsetof(Vertex, Vertex::uv);
			attributes.push_back(attr);

			s_Data->geometryPipeline = std::make_shared<Core::GraphicsPipeline>("shaders/geometryPass.vert.spv", "shaders/geometryPass.frag.spv",
				bdesc, attributes, sizeof(Data::Constant),
				std::vector<vk::DescriptorSetLayout>({ s_Data->CamDescriptorSetLayout, Model::s_MaterialDescriptorSetLayout }), s_Data->geometryRenderPass->GetRenderPass());
		}
		//SkyBoxPipeline
		{
			vk::VertexInputBindingDescription bdesc;
			std::vector<vk::VertexInputAttributeDescription> attributes;

			bdesc.binding = 0;
			bdesc.inputRate = vk::VertexInputRate::eVertex;
			bdesc.stride = sizeof(HML::Vector3<>) + sizeof(HML::Vector2<>);

			vk::VertexInputAttributeDescription attr;
			attr.binding = 0;
			attr.location = 0;
			attr.format = vk::Format::eR32G32B32Sfloat;
			attr.offset = 0;
			attributes.push_back(attr);

			attr.binding = 0;
			attr.location = 1;
			attr.format = vk::Format::eR32G32Sfloat;
			attr.offset = sizeof(HML::Vector3<>);
			attributes.push_back(attr);

			s_Data->skyboxPipeline = std::make_shared<Core::GraphicsPipeline>("shaders/skyBox.vert.spv", "shaders/skyBox.frag.spv",
				bdesc, attributes, 0, std::vector<vk::DescriptorSetLayout>({ s_Data->CamDescriptorSetLayout }), s_Data->geometryRenderPass->GetRenderPass(), true, true);
			s_Data->compositePipeline = std::make_shared<Core::GraphicsPipeline>("shaders/composite.vert.spv", "shaders/composite.frag.spv",
				bdesc, attributes, sizeof(HML::Vector2<>), std::vector<vk::DescriptorSetLayout>({s_Data->CompositeDescriptorSetLayout}), s_Data->compositeRenderPass->GetRenderPass(), true, true);
		}
		s_Data->offScreenFrameBuffer = std::make_shared<Core::FrameBuffer>(s_Data->geometryRenderPass->GetRenderPass());
		s_Data->swapchainTarget = std::make_shared<Core::FrameBuffer>(s_Data->compositeRenderPass->GetRenderPass(), true);
		vk::CommandBufferAllocateInfo allocInfo{};
		allocInfo.commandPool = Core::Context::_graphicsCommandPool;
		allocInfo.level = vk::CommandBufferLevel::ePrimary;
		allocInfo.commandBufferCount = s_Data->swapchainTarget->GetFramesInFlightCount();
		s_Data->cmd = Core::Context::_device->allocateCommandBuffers(allocInfo);

		vk::WriteDescriptorSet write;
		vk::DescriptorImageInfo desc_image;

		desc_image.sampler = s_Data->offScreenFrameBuffer->GetFrameBufferAttachment(0, 0).Sampler;
		desc_image.imageView = s_Data->offScreenFrameBuffer->GetFrameBufferAttachment(0, 0).View;
		desc_image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		write.sType = vk::StructureType::eWriteDescriptorSet;
		write.dstSet = s_Data->CompositeDescriptorSet;
		write.dstBinding = 0;
		write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		write.descriptorCount = 1;
		write.pBufferInfo = nullptr;
		write.pImageInfo = &desc_image;

		Core::Context::_device->updateDescriptorSets(write, nullptr);
	}

	PBR::PBR()
	{
		Texture::Init();
		Model::Init();
		s_Data->pushConstant.ModelTransform = HML::Mat4x4<>(1.0f);
		s_Data->pushConstant.ModelTransform = HML::Transform::Rotate(HML::Mat4x4<>(1.0f), s_Data->rotation, HML::Vector3<>(0.0f, 0.0f, 1.0f));
		s_Data->pushConstant.ModelTransform = HML::Transform::Rotate(s_Data->pushConstant.ModelTransform, s_Data->rotation, HML::Vector3<>(1.0f, 0.0f, 0.0f));
		s_Data->pushConstant.ModelTransform = HML::Transform::Translate(s_Data->pushConstant.ModelTransform, HML::Vector3<>(0.0f, 1.0f, 0.0f));
		s_Data->pushConstant.ModelTransform = HML::Transform::Scale(s_Data->pushConstant.ModelTransform, HML::Vector3<>(0.005f, 0.005f, 0.005f));
		s_Data->axe = std::make_shared<Model>("res/Axe/scene.gltf");
		s_Data->brdfLut = std::make_shared<Texture>("res/BRDF_LUT.tga", false);
		s_Data->environmentMap = std::make_shared<EnvironmentMap>("res/kloofendal_43d_clear_puresky_4k.hdr");
		s_Data->cam = std::make_shared<Camera>(HML::Radians(45.0f), float(Core::Context::_swapchain.extent.width) 
			/ float(Core::Context::_swapchain.extent.height), 0.1f, 1000.0f);

		std::vector<vk::DescriptorSetLayoutBinding> bindings;

		vk::DescriptorSetLayoutBinding binding;
		binding.binding = 0;
		binding.descriptorType = vk::DescriptorType::eUniformBuffer;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

		bindings.push_back(binding);

		binding.binding = 1;
		binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		bindings.push_back(binding);

		binding.binding = 2;
		binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		bindings.push_back(binding);

		binding.binding = 3;
		binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		bindings.push_back(binding);

		vk::DescriptorSetLayoutCreateInfo layoutinfo;
		layoutinfo.bindingCount = bindings.size();
		layoutinfo.pBindings = bindings.data();

		s_Data->CamDescriptorSetLayout = Core::Context::_device->createDescriptorSetLayout(layoutinfo);

		vk::DescriptorSetAllocateInfo info;
		info.sType = vk::StructureType::eDescriptorSetAllocateInfo;
		info.descriptorPool = Core::Context::_descriptorPool;
		info.descriptorSetCount = 1;
		info.pSetLayouts = &s_Data->CamDescriptorSetLayout;

		s_Data->CamDescriptorSet = Core::Context::_device->allocateDescriptorSets(info).front();

		vk::DescriptorBufferInfo bufferinfo;
		bufferinfo.buffer = s_Data->cam->m_CameraUniformBuffer.buffer;
		bufferinfo.offset = 0;
		bufferinfo.range = sizeof(CameraData);
		vk::WriteDescriptorSet write;
		vk::DescriptorImageInfo desc_image;
		write.sType = vk::StructureType::eWriteDescriptorSet;
		write.dstSet = s_Data->CamDescriptorSet;
		write.dstBinding = 0;
		write.descriptorType = vk::DescriptorType::eUniformBuffer;
		write.pBufferInfo = &bufferinfo;
		write.descriptorCount = 1;

		Core::Context::_device->updateDescriptorSets(write, nullptr);

		desc_image.sampler = s_Data->environmentMap->GetRadianceMap().sampler;
		desc_image.imageView = s_Data->environmentMap->GetRadianceMap().view;
		desc_image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		write.sType = vk::StructureType::eWriteDescriptorSet;
		write.dstSet = s_Data->CamDescriptorSet;
		write.dstBinding = 1;
		write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		write.descriptorCount = 1;
		write.pBufferInfo = nullptr;
		write.pImageInfo = &desc_image;

		Core::Context::_device->updateDescriptorSets(write, nullptr);

		desc_image.sampler = s_Data->environmentMap->GetIrradianceMap().sampler;
		desc_image.imageView = s_Data->environmentMap->GetIrradianceMap().view;
		desc_image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		write.sType = vk::StructureType::eWriteDescriptorSet;
		write.dstSet = s_Data->CamDescriptorSet;
		write.dstBinding = 2;
		write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		write.descriptorCount = 1;
		write.pBufferInfo = nullptr;
		write.pImageInfo = &desc_image;

		Core::Context::_device->updateDescriptorSets(write, nullptr);

		desc_image.sampler = s_Data->brdfLut->GetSampler();
		desc_image.imageView = s_Data->brdfLut->GetImageView();
		desc_image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		write.sType = vk::StructureType::eWriteDescriptorSet;
		write.dstSet = s_Data->CamDescriptorSet;
		write.dstBinding = 3;
		write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		write.descriptorCount = 1;
		write.pBufferInfo = nullptr;
		write.pImageInfo = &desc_image;

		Core::Context::_device->updateDescriptorSets(write, nullptr);

		bindings.clear();

		binding.binding = 0;
		binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		bindings.push_back(binding);

		layoutinfo.bindingCount = bindings.size();
		layoutinfo.pBindings = bindings.data();

		s_Data->CompositeDescriptorSetLayout = Core::Context::_device->createDescriptorSetLayout(layoutinfo);

		info.sType = vk::StructureType::eDescriptorSetAllocateInfo;
		info.descriptorPool = Core::Context::_descriptorPool;
		info.descriptorSetCount = 1;
		info.pSetLayouts = &s_Data->CompositeDescriptorSetLayout;

		s_Data->CompositeDescriptorSet = Core::Context::_device->allocateDescriptorSets(info).front();

		Create();
	}

	void PBR::OnUpdate()
	{
		s_Data->cam->OnUpdate();

		vk::CommandBufferBeginInfo beginInfo{};

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].begin(beginInfo);

		vk::RenderPassBeginInfo renderPassInfo{};
		renderPassInfo.renderPass = s_Data->geometryRenderPass->GetRenderPass();
		renderPassInfo.framebuffer = s_Data->offScreenFrameBuffer->GetFrameBuffer();
		renderPassInfo.renderArea.offset.x = 0;
		renderPassInfo.renderArea.offset.y = 0;
		renderPassInfo.renderArea.extent = Core::Context::_swapchain.extent;

		vk::ClearValue clearColor{ std::array<float, 4>{0.2f, 0.5f, 0.7f, 1.0f} };
		vk::ClearValue clearDepth;
		clearDepth.depthStencil.depth = 1.0f;
		std::vector<vk::ClearValue> values{ clearColor, clearDepth };
		renderPassInfo.clearValueCount = values.size();
		renderPassInfo.pClearValues = values.data();
		std::vector<vk::DescriptorSet> sets = { s_Data->CamDescriptorSet };

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindPipeline(vk::PipelineBindPoint::eGraphics, s_Data->skyboxPipeline->GetPipeline());

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, s_Data->skyboxPipeline->GetLayout(), 0, sets, {});

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindVertexBuffers(0, s_Data->quadVertexBuffer,{ 0 });

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindIndexBuffer(s_Data->quadIndexBuffer, 0, vk::IndexType::eUint32);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].drawIndexed(6, 1, 0, 0, 0);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindPipeline(vk::PipelineBindPoint::eGraphics, s_Data->geometryPipeline->GetPipeline());

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].pushConstants(s_Data->geometryPipeline->GetLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(Data::Constant), &s_Data->pushConstant);
		
		s_Data->axe->DrawModel(s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage], s_Data->geometryPipeline->GetLayout(), sets);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].endRenderPass();

		renderPassInfo.renderPass = s_Data->compositeRenderPass->GetRenderPass();
		renderPassInfo.framebuffer = s_Data->swapchainTarget->GetFrameBuffer();
		renderPassInfo.renderArea.offset.x = 0;
		renderPassInfo.renderArea.offset.y = 0;
		renderPassInfo.renderArea.extent = Core::Context::_swapchain.extent;

		sets.clear();
		sets.push_back(s_Data->CompositeDescriptorSet);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindPipeline(vk::PipelineBindPoint::eGraphics, s_Data->compositePipeline->GetPipeline());

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, s_Data->compositePipeline->GetLayout(), 0, sets, {});

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindVertexBuffers(0, s_Data->quadVertexBuffer, { 0 });

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindIndexBuffer(s_Data->quadIndexBuffer, 0, vk::IndexType::eUint32);

		HML::Vector2<> resolution = { (float)Core::Context::_swapchain.extent.width, (float)Core::Context::_swapchain.extent.height };

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].pushConstants(s_Data->geometryPipeline->GetLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(HML::Vector2<>), &resolution);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].drawIndexed(6, 1, 0, 0, 0);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].endRenderPass();

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].end();

		vk::SubmitInfo submit_info{};
		submit_info.sType = vk::StructureType::eSubmitInfo;
		vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		submit_info.pWaitDstStageMask = &waitStage;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &Core::Context::imageAvailableSemaphores[Core::Context::_swapchain.currentFrame];
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &Core::Context::renderFinishedSemaphores[Core::Context::_swapchain.currentFrame];;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage];

		Core::Context::_device->resetFences(Core::Context::inFlightFences[Core::Context::_swapchain.currentFrame]);

		Core::Context::_graphicsQueue.submit(submit_info, Core::Context::inFlightFences[Core::Context::_swapchain.currentFrame]);
	}

	void PBR::Shutdown()
	{
		s_Data->cam->Destroy();
		s_Data->brdfLut->Destroy();
		s_Data->environmentMap->Destroy();
		s_Data->axe->Destroy();
		Core::Context::_device->destroyDescriptorSetLayout(s_Data->CamDescriptorSetLayout);
		Core::Context::_device->freeDescriptorSets(Core::Context::_descriptorPool, s_Data->CamDescriptorSet);
		Destroy();
		delete s_Data;
	}

	void PBR::Destroy()
	{
		s_Data->skyboxPipeline->Destroy();
		s_Data->geometryPipeline->Destroy();
		s_Data->compositePipeline->Destroy();
		s_Data->offScreenFrameBuffer->Destroy();
		s_Data->swapchainTarget->Destroy();
		Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, s_Data->cmd);
		s_Data->cmd.clear();
		s_Data->geometryRenderPass->Destroy();
		s_Data->compositeRenderPass->Destroy();
	}

	void PBR::Recreate()
	{
		Create();
	}
}