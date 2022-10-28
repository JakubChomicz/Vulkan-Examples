#include "PlayerController.hpp"
#include <HML/HeaderMath.hpp>
#include "API/VulkanInit.hpp"
#include "App.hpp"
#include <string>
#include <fstream>
#include "Model.hpp"
#include "Camera.hpp"
#include "Texture.hpp"

namespace Example
{
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
	struct Data
	{
		struct DepthAttachment
		{
			vk::Image image;
			vk::ImageView view;
			vk::MemoryRequirements MemRequs;
			vk::DeviceMemory Memory;
		};
		struct Constant
		{
			HML::Mat4x4<> ModelTransform;
		};
		vk::VertexInputBindingDescription bdesc;
		std::vector<vk::VertexInputAttributeDescription> attributes;
		vk::PipelineLayout pipelineLayout;
		vk::Pipeline graphicsPipeline;
		vk::RenderPass renderpass;
		std::vector<vk::Framebuffer> framebuffers;
		std::vector<vk::CommandBuffer> cmd;
		std::string vertex = "shaders/vertex.spv";
		std::string fragment = "shaders/fragment.spv";
		//World Data
		HML::Vector3<> Position = {0.0f, 0.0f, 5.0f};
		Constant pushConstant;
		float rotation = 0.01f;
		std::shared_ptr<Model> treeModel;
		std::vector<DepthAttachment> depth_attachment;
		std::shared_ptr<Camera> cam;
		vk::DescriptorSetLayout CamDescriptorSetLayout;
		vk::DescriptorSet CamDescriptorSet;
		std::shared_ptr<Texture> treeTexture;
	};
	static Data* s_Data = new Data();
	static void Create()
	{
		Data::DepthAttachment dAttachment;
		for (int i = 0; i < Core::Context::_swapchain.swapchainImageViews.size(); ++i)
		{
			vk::ImageCreateInfo imageInfo;
			imageInfo.imageType = vk::ImageType::e2D;
			imageInfo.extent.width = Core::Context::_swapchain.swapchainWidth;
			imageInfo.extent.height = Core::Context::_swapchain.swapchainHeight;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = vk::Format::eD32SfloatS8Uint;
			imageInfo.tiling = vk::ImageTiling::eOptimal;
			imageInfo.initialLayout = vk::ImageLayout::eUndefined;
			imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
			imageInfo.sharingMode = vk::SharingMode::eExclusive;
			imageInfo.samples = vk::SampleCountFlagBits::e1;

			dAttachment.image = Core::Context::_device->createImage(imageInfo);
			dAttachment.MemRequs = Core::Context::_device->getImageMemoryRequirements(dAttachment.image);
			vk::MemoryAllocateInfo memAlloc;
			memAlloc.sType = vk::StructureType::eMemoryAllocateInfo;
			memAlloc.allocationSize = dAttachment.MemRequs.size;
			memAlloc.memoryTypeIndex = Core::Context::findMemoryType(dAttachment.MemRequs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);		//FIX
			dAttachment.Memory = Core::Context::_device->allocateMemory(memAlloc);
			Core::Context::_device->bindImageMemory(dAttachment.image, dAttachment.Memory, 0);

			vk::ImageViewCreateInfo viewInfo;
			viewInfo.viewType = vk::ImageViewType::e2D;
			viewInfo.format = vk::Format::eD32SfloatS8Uint;
			viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			viewInfo.image = dAttachment.image;
			dAttachment.view = Core::Context::_device->createImageView(viewInfo);
			
			s_Data->depth_attachment.push_back(dAttachment);
		}
		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

		vk::PushConstantRange range;
		range.offset = 0;
		range.size = sizeof(Data::Constant);
		range.stageFlags = vk::ShaderStageFlagBits::eVertex;

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.flags = vk::PipelineVertexInputStateCreateFlags();
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &s_Data->bdesc;
		vertexInputInfo.vertexAttributeDescriptionCount = s_Data->attributes.size();
		vertexInputInfo.pVertexAttributeDescriptions = s_Data->attributes.data();

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		inputAssemblyInfo.flags = vk::PipelineInputAssemblyStateCreateFlags();
		inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;

#ifdef M_DEBUG
		std::cout << "Create vertex shader module" << std::endl;
#endif
		vk::ShaderModule vertexShader = createShaderModule(s_Data->vertex);
		vk::PipelineShaderStageCreateInfo vertexShaderInfo{};
		vertexShaderInfo.flags = vk::PipelineShaderStageCreateFlags();
		vertexShaderInfo.stage = vk::ShaderStageFlagBits::eVertex;
		vertexShaderInfo.module = vertexShader;
		vertexShaderInfo.pName = "main";
		shaderStages.push_back(vertexShaderInfo);

		//Viewport and Scissor
		vk::Viewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)Core::Context::_swapchain.extent.width;
		viewport.height = (float)Core::Context::_swapchain.extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vk::Rect2D scissor = {};
		scissor.offset.x = 0.0f;
		scissor.offset.y = 0.0f;
		scissor.extent = Core::Context::_swapchain.extent;
		vk::PipelineViewportStateCreateInfo viewportState{};
		viewportState.flags = vk::PipelineViewportStateCreateFlags();
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		//Rasterizer
		vk::PipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.flags = vk::PipelineRasterizationStateCreateFlags();
		rasterizer.depthClampEnable = VK_FALSE; //discard out of bounds fragments, don't clamp them
		rasterizer.rasterizerDiscardEnable = VK_FALSE; //This flag would disable fragment output
		rasterizer.polygonMode = vk::PolygonMode::eFill;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = vk::CullModeFlagBits::eBack;
		rasterizer.frontFace = vk::FrontFace::eClockwise;
		rasterizer.depthBiasEnable = VK_FALSE; //Depth bias can be useful in shadow maps.

		//Fragment Shader
#ifdef M_DEBUG
		std::cout << "Create fragment shader module" << std::endl;
#endif
		vk::ShaderModule fragmentShader = createShaderModule(s_Data->fragment);
		vk::PipelineShaderStageCreateInfo fragmentShaderInfo{};
		fragmentShaderInfo.flags = vk::PipelineShaderStageCreateFlags();
		fragmentShaderInfo.stage = vk::ShaderStageFlagBits::eFragment;
		fragmentShaderInfo.module = fragmentShader;
		fragmentShaderInfo.pName = "main";
		shaderStages.push_back(fragmentShaderInfo);
		//Now both shaders have been made, we can declare them to the pipeline info

		//Multisampling
		vk::PipelineMultisampleStateCreateInfo multisampling{};
		multisampling.flags = vk::PipelineMultisampleStateCreateFlags();
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

		//Color Blend
		vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		colorBlendAttachment.blendEnable = VK_FALSE;
		vk::PipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.flags = vk::PipelineColorBlendStateCreateFlags();
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = vk::LogicOp::eCopy;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		vk::PipelineDepthStencilStateCreateInfo depthStencilInfo;
		depthStencilInfo.sType = vk::StructureType::ePipelineDepthStencilStateCreateInfo;
		depthStencilInfo.depthTestEnable = true;
		depthStencilInfo.depthWriteEnable = true;
		depthStencilInfo.depthCompareOp = vk::CompareOp::eLess;
		depthStencilInfo.depthBoundsTestEnable = false;
		depthStencilInfo.stencilTestEnable = false;

		//Pipeline Layout
#ifdef M_DEBUG
		std::cout << "Create Pipeline Layout" << std::endl;
#endif
		std::array<vk::DescriptorSetLayout, 2> layouts = { s_Data->CamDescriptorSetLayout, s_Data->treeTexture->m_layout };

		vk::PipelineLayoutCreateInfo layoutInfo;
		layoutInfo.flags = vk::PipelineLayoutCreateFlags();
		layoutInfo.setLayoutCount = layouts.size();
		layoutInfo.pSetLayouts = layouts.data();
		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &range;

		s_Data->pipelineLayout = Core::Context::_device->createPipelineLayout(layoutInfo);


		//Renderpass
#ifdef M_DEBUG
		std::cout << "Create RenderPass" << std::endl;
#endif
		vk::AttachmentDescription colorAttachment{};
		colorAttachment.flags = vk::AttachmentDescriptionFlags();
		colorAttachment.format = vk::Format::eB8G8R8A8Unorm;
		colorAttachment.samples = vk::SampleCountFlagBits::e1;
		colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
		colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

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
		subpass.pDepthStencilAttachment = &depthStencilAttachmentRef;

		std::array<vk::AttachmentDescription, 2> attachments{ colorAttachment, depthAttachment };

		//Now create the renderpass
		vk::RenderPassCreateInfo renderpassInfo{};
		renderpassInfo.flags = vk::RenderPassCreateFlags();
		renderpassInfo.attachmentCount = attachments.size();
		renderpassInfo.pAttachments = attachments.data();
		renderpassInfo.subpassCount = 1;
		renderpassInfo.pSubpasses = &subpass;
		s_Data->renderpass = Core::Context::_device->createRenderPass(renderpassInfo);


		//Extra stuff

		//Make the Pipeline
#ifdef M_DEBUG
		std::cout << "Create Graphics Pipeline" << std::endl;
#endif
		vk::GraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.flags = vk::PipelineCreateFlags();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.stageCount = shaderStages.size();
		pipelineInfo.pStages = shaderStages.data();
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDepthStencilState = &depthStencilInfo;
		pipelineInfo.layout = s_Data->pipelineLayout;
		pipelineInfo.renderPass = s_Data->renderpass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = nullptr;

		s_Data->graphicsPipeline = Core::Context::_device->createGraphicsPipeline(nullptr, pipelineInfo).value;

		Core::Context::_device->destroyShaderModule(vertexShader);
		Core::Context::_device->destroyShaderModule(fragmentShader);

		for (int i = 0; i < Core::Context::_swapchain.swapchainImageViews.size(); ++i) {

			std::vector<vk::ImageView> attchs = { Core::Context::_swapchain.swapchainImageViews[i], s_Data->depth_attachment[i].view };
			vk::FramebufferCreateInfo framebufferInfo;
			framebufferInfo.flags = vk::FramebufferCreateFlags();
			framebufferInfo.renderPass = s_Data->renderpass;
			framebufferInfo.attachmentCount = attchs.size();
			framebufferInfo.pAttachments = attchs.data();
			framebufferInfo.width = Core::Context::_swapchain.extent.width;
			framebufferInfo.height = Core::Context::_swapchain.extent.height;
			framebufferInfo.layers = 1;
			s_Data->framebuffers.push_back(Core::Context::_device->createFramebuffer(framebufferInfo));
		}
		vk::CommandBufferAllocateInfo allocInfo{};
		allocInfo.commandPool = Core::Context::_graphicsCommandPool;
		allocInfo.level = vk::CommandBufferLevel::ePrimary;
		allocInfo.commandBufferCount = s_Data->framebuffers.size();
		s_Data->cmd = Core::Context::_device->allocateCommandBuffers(allocInfo);
	}
	PlayerController::PlayerController()
	{
		s_Data->cam = std::make_shared<Camera>(HML::Radians(45.0f), float(Core::Context::_swapchain.extent.width) 
			/ float(Core::Context::_swapchain.extent.height), 0.1f, 1000.0f);

		vk::DescriptorSetLayoutBinding binding;
		binding.binding = 0;
		binding.descriptorType = vk::DescriptorType::eUniformBuffer;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

		vk::DescriptorSetLayoutCreateInfo layoutinfo;
		layoutinfo.bindingCount = 1;
		layoutinfo.pBindings = &binding;

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
		write.sType = vk::StructureType::eWriteDescriptorSet;
		write.dstSet = s_Data->CamDescriptorSet;
		write.dstBinding = 0;
		write.descriptorType = vk::DescriptorType::eUniformBuffer;
		write.pBufferInfo = &bufferinfo;
		write.descriptorCount = 1;

		Core::Context::_device->updateDescriptorSets(write, nullptr);

		s_Data->pushConstant.ModelTransform = HML::Mat4x4<>(1.0f);
		s_Data->treeModel = std::make_shared<Model>("res/Tree.obj");

		s_Data->treeTexture = std::make_shared<Texture>("res/treeTex.png");

		s_Data->bdesc.binding = 0;
		s_Data->bdesc.inputRate = vk::VertexInputRate::eVertex;
		s_Data->bdesc.stride = sizeof(Vertex);

		vk::VertexInputAttributeDescription attr01;
		attr01.binding = 0;
		attr01.location = 0;
		attr01.format = vk::Format::eR32G32B32Sfloat;
		attr01.offset = offsetof(Vertex, Vertex::position);;
		s_Data->attributes.push_back(attr01);

		vk::VertexInputAttributeDescription attr02;
		attr02.binding = 0;
		attr02.location = 1;
		attr02.format = vk::Format::eR32G32B32Sfloat;
		attr02.offset = offsetof(Vertex, Vertex::normal);
		s_Data->attributes.push_back(attr02);

		vk::VertexInputAttributeDescription attr03;
		attr01.binding = 0;
		attr01.location = 2;
		attr01.format = vk::Format::eR32G32B32A32Sfloat;
		attr01.offset = offsetof(Vertex, Vertex::tangent);
		s_Data->attributes.push_back(attr01);

		vk::VertexInputAttributeDescription attr04;
		attr02.binding = 0;
		attr02.location = 3;
		attr02.format = vk::Format::eR32G32B32A32Sfloat;
		attr02.offset = offsetof(Vertex, Vertex::bitangent);
		s_Data->attributes.push_back(attr02);

		vk::VertexInputAttributeDescription attr05;
		attr02.binding = 0;
		attr02.location = 4;
		attr02.format = vk::Format::eR32G32B32A32Sfloat;
		attr02.offset = offsetof(Vertex, Vertex::color);
		s_Data->attributes.push_back(attr02);

		vk::VertexInputAttributeDescription attr06;
		attr02.binding = 0;
		attr02.location = 5;
		attr02.format = vk::Format::eR32G32Sfloat;
		attr02.offset = offsetof(Vertex, Vertex::uv);
		s_Data->attributes.push_back(attr02);

		Create();
	}
	void PlayerController::OnUpdate()
	{
		//s_Data->pushConstant.ModelTransform = HML::Transform::Rotate(s_Data->pushConstant.ModelTransform, s_Data->rotation, HML::Vector3<>(0.0f, 1.0f, 0.0f));	//Uncomment if u want spinning tree

		s_Data->cam->OnUpdate();

		vk::CommandBufferBeginInfo beginInfo{};

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].begin(beginInfo);

		vk::RenderPassBeginInfo renderPassInfo{};
		renderPassInfo.renderPass = s_Data->renderpass;
		renderPassInfo.framebuffer = s_Data->framebuffers[Core::Context::_swapchain.currentFrame];
		renderPassInfo.renderArea.offset.x = 0;
		renderPassInfo.renderArea.offset.y = 0;
		renderPassInfo.renderArea.extent = Core::Context::_swapchain.extent;

		vk::ClearValue clearColor{ std::array<float, 4>{0.2f, 0.5f, 0.7f, 1.0f} };
		vk::ClearValue clearDepth;
		clearDepth.depthStencil.depth = 1.0f;
		std::vector<vk::ClearValue> values { clearColor, clearDepth };
		renderPassInfo.clearValueCount = values.size();
		renderPassInfo.pClearValues = values.data();

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindPipeline(vk::PipelineBindPoint::eGraphics, s_Data->graphicsPipeline);

		std::array<vk::DescriptorSet, 2> sets = { s_Data->CamDescriptorSet, s_Data->treeTexture->m_set };
		std::array<uint32_t, 2> offsets = { 0, 0 };

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, s_Data->pipelineLayout, 0, sets, {  });

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].pushConstants(s_Data->pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(Data::Constant), &s_Data->pushConstant);
		
		s_Data->treeModel->DrawModel(s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage]);

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
	void PlayerController::Shutdown()
	{
		s_Data->cam->Destroy();
		s_Data->treeModel->Destroy();
		s_Data->treeTexture->Destroy();
		Destroy();
		delete s_Data;
	}
	void PlayerController::Destroy()
	{
		Core::Context::_device->destroyDescriptorSetLayout(s_Data->CamDescriptorSetLayout);
		Core::Context::_device->freeDescriptorSets(Core::Context::_descriptorPool, s_Data->CamDescriptorSet);
		Core::Context::_device->destroyPipelineLayout(s_Data->pipelineLayout);
		Core::Context::_device->destroyPipeline(s_Data->graphicsPipeline);
		for (auto framebuffer : s_Data->framebuffers)
			Core::Context::_device->destroyFramebuffer(framebuffer);
		s_Data->framebuffers.clear();
		for (auto attachment : s_Data->depth_attachment)
		{
			Core::Context::_device->destroyImageView(attachment.view);
			Core::Context::_device->freeMemory(attachment.Memory);
			Core::Context::_device->destroyImage(attachment.image);
		}
		s_Data->depth_attachment.clear();
		Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, s_Data->cmd);
		s_Data->cmd.clear();
		Core::Context::_device->destroyRenderPass(s_Data->renderpass);
	}
	void PlayerController::Recreate()
	{
		Create();
	}
}