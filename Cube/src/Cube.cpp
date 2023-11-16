#include "Cube.hpp"
#include <HML/HeaderMath.hpp>
#include "API/VulkanInit.hpp"
#include "App.hpp"
#include <string>
#include <fstream>

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
		return Core::Context::Get()->GetDevice()->createShaderModule(moduleInfo);
	}
	struct Data
	{
		struct Constant
		{
			HML::Mat4x4<> ViewProjection;
		};
		vk::VertexInputBindingDescription bdesc;
		std::vector<vk::VertexInputAttributeDescription> attributes;
		vk::Buffer vertexBuffer;
		vk::DeviceMemory veretxBufferMemory;
		vk::PipelineLayout pipelineLayout;
		vk::Pipeline graphicsPipeline;
		vk::RenderPass renderpass;
		std::vector<vk::Framebuffer> framebuffers;
		std::vector<vk::CommandBuffer> cmd;
		std::string vertex = "shaders/vertex.spv";
		std::string fragment = "shaders/fragment.spv";
		//World Data
		HML::Mat4x4<> Model;
		HML::Vector3<> Position = {0.0f, 0.0f, 5.0f};
		std::vector<HML::Vector3<>> verts;
		std::vector<int> indices;
		Constant pushConstant;
		float rotation = 0.01f;
	};
	static Data* s_Data = new Data();
	static void Create()
	{
		auto View = HML::Transform::View_RH(s_Data->Position, HML::Vector3<>(0, 0, 0), HML::Vector3<>(0, -1, 0));	//View Matrix
		auto Projection = 
			HML::ClipSpace::Perspective_RH_ZO(HML::Radians(45.0f), float(Core::Context::Get()->GetSwapchain().extent.width)
				/ float(Core::Context::Get()->GetSwapchain().extent.height), 0.1f, 1000.0f);	//Projection Matrix

		s_Data->pushConstant.ViewProjection = Projection * View;
		vk::GraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.flags = vk::PipelineCreateFlags();

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
		pipelineInfo.pVertexInputState = &vertexInputInfo;

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		inputAssemblyInfo.flags = vk::PipelineInputAssemblyStateCreateFlags();
		inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;
		pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;

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
		viewport.width = (float)Core::Context::Get()->GetSwapchain().extent.width;
		viewport.height = (float)Core::Context::Get()->GetSwapchain().extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vk::Rect2D scissor = {};
		scissor.offset.x = 0.0f;
		scissor.offset.y = 0.0f;
		scissor.extent = Core::Context::Get()->GetSwapchain().extent;
		vk::PipelineViewportStateCreateInfo viewportState{};
		viewportState.flags = vk::PipelineViewportStateCreateFlags();
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;
		pipelineInfo.pViewportState = &viewportState;

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
		pipelineInfo.pRasterizationState = &rasterizer;

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
		pipelineInfo.stageCount = shaderStages.size();
		pipelineInfo.pStages = shaderStages.data();

		//Multisampling
		vk::PipelineMultisampleStateCreateInfo multisampling{};
		multisampling.flags = vk::PipelineMultisampleStateCreateFlags();
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
		pipelineInfo.pMultisampleState = &multisampling;

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
		pipelineInfo.pColorBlendState = &colorBlending;

		//Pipeline Layout
#ifdef M_DEBUG
		std::cout << "Create Pipeline Layout" << std::endl;
#endif
		vk::PipelineLayoutCreateInfo layoutInfo;
		layoutInfo.flags = vk::PipelineLayoutCreateFlags();
		layoutInfo.setLayoutCount = 0;
		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &range;

		s_Data->pipelineLayout = Core::Context::Get()->GetDevice()->createPipelineLayout(layoutInfo);

		pipelineInfo.layout = s_Data->pipelineLayout;

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

		//Declare that attachment to be color buffer 0 of the framebuffer
		vk::AttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

		//Renderpasses are broken down into subpasses, there's always at least one.
		vk::SubpassDescription subpass{};
		subpass.flags = vk::SubpassDescriptionFlags();
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		//Now create the renderpass
		vk::RenderPassCreateInfo renderpassInfo{};
		renderpassInfo.flags = vk::RenderPassCreateFlags();
		renderpassInfo.attachmentCount = 1;
		renderpassInfo.pAttachments = &colorAttachment;
		renderpassInfo.subpassCount = 1;
		renderpassInfo.pSubpasses = &subpass;
		s_Data->renderpass = Core::Context::Get()->GetDevice()->createRenderPass(renderpassInfo);

		pipelineInfo.renderPass = s_Data->renderpass;
		pipelineInfo.subpass = 0;

		//Extra stuff
		pipelineInfo.basePipelineHandle = nullptr;

		//Make the Pipeline
#ifdef M_DEBUG
		std::cout << "Create Graphics Pipeline" << std::endl;
#endif
		s_Data->graphicsPipeline = Core::Context::Get()->GetDevice()->createGraphicsPipeline(nullptr, pipelineInfo).value;

		Core::Context::Get()->GetDevice()->destroyShaderModule(vertexShader);
		Core::Context::Get()->GetDevice()->destroyShaderModule(fragmentShader);

		for (int i = 0; i < Core::Context::Get()->GetSwapchain().swapchainImageViews.size(); ++i) {

			vk::FramebufferCreateInfo framebufferInfo;
			framebufferInfo.flags = vk::FramebufferCreateFlags();
			framebufferInfo.renderPass = s_Data->renderpass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = &Core::Context::Get()->GetSwapchain().swapchainImageViews[i];
			framebufferInfo.width = Core::Context::Get()->GetSwapchain().extent.width;
			framebufferInfo.height = Core::Context::Get()->GetSwapchain().extent.height;
			framebufferInfo.layers = 1;
			s_Data->framebuffers.push_back(Core::Context::Get()->GetDevice()->createFramebuffer(framebufferInfo));
		}
		vk::CommandBufferAllocateInfo allocInfo{};
		allocInfo.commandPool = Core::Context::Get()->GetGraphicsCommandPool();
		allocInfo.level = vk::CommandBufferLevel::ePrimary;
		allocInfo.commandBufferCount = s_Data->framebuffers.size();
		s_Data->cmd = Core::Context::Get()->GetDevice()->allocateCommandBuffers(allocInfo);
	}
	Cube::Cube()
	{
		float zPos = 0.0f;
		s_Data->verts = {
		{-0.5f,-0.5f,-0.5f + zPos}, {1.0f, 0.0f, 0.0f},	// -X side
		{-0.5f,-0.5f, 0.5f + zPos}, {1.0f, 0.0f, 0.0f},
		{-0.5f, 0.5f, 0.5f + zPos}, {1.0f, 0.0f, 0.0f},
		{-0.5f, 0.5f, 0.5f + zPos}, {1.0f, 0.0f, 0.0f},
		{-0.5f, 0.5f,-0.5f + zPos}, {1.0f, 0.0f, 0.0f},
		{-0.5f,-0.5f,-0.5f + zPos}, {1.0f, 0.0f, 0.0f},
		
		{-0.5f,-0.5f,-0.5f + zPos}, {0.0f, 1.0f, 0.0f},	// -Z side
		{ 0.5f, 0.5f,-0.5f + zPos}, {0.0f, 1.0f, 0.0f},
		{ 0.5f,-0.5f,-0.5f + zPos}, {0.0f, 1.0f, 0.0f},
		{-0.5f,-0.5f,-0.5f + zPos}, {0.0f, 1.0f, 0.0f},
		{-0.5f, 0.5f,-0.5f + zPos}, {0.0f, 1.0f, 0.0f},
		{ 0.5f, 0.5f,-0.5f + zPos}, {0.0f, 1.0f, 0.0f},
		
		{-0.5f,-0.5f,-0.5f + zPos}, {0.0f, 0.0f, 1.0f},	// -Y side
		{ 0.5f,-0.5f,-0.5f + zPos}, {0.0f, 0.0f, 1.0f},
		{ 0.5f,-0.5f, 0.5f + zPos}, {0.0f, 0.0f, 1.0f},
		{-0.5f,-0.5f,-0.5f + zPos}, {0.0f, 0.0f, 1.0f},
		{ 0.5f,-0.5f, 0.5f + zPos}, {0.0f, 0.0f, 1.0f},
		{-0.5f,-0.5f, 0.5f + zPos}, {0.0f, 0.0f, 1.0f},
		
		{-0.5f, 0.5f,-0.5f + zPos}, {0.0f, 0.0f, 1.0f}, // +Y side
		{-0.5f, 0.5f, 0.5f + zPos}, {0.0f, 0.0f, 1.0f},
		{ 0.5f, 0.5f, 0.5f + zPos}, {0.0f, 0.0f, 1.0f},
		{-0.5f, 0.5f,-0.5f + zPos}, {0.0f, 0.0f, 1.0f},
		{ 0.5f, 0.5f, 0.5f + zPos}, {0.0f, 0.0f, 1.0f},
		{ 0.5f, 0.5f,-0.5f + zPos}, {0.0f, 0.0f, 1.0f},
		
		 {0.5f, 0.5f,-0.5f + zPos}, {1.0f, 0.0f, 0.0f}, // +X side
		 {0.5f, 0.5f, 0.5f + zPos}, {1.0f, 0.0f, 0.0f},
		 {0.5f,-0.5f, 0.5f + zPos}, {1.0f, 0.0f, 0.0f},
		 {0.5f,-0.5f, 0.5f + zPos}, {1.0f, 0.0f, 0.0f},
		 {0.5f,-0.5f,-0.5f + zPos}, {1.0f, 0.0f, 0.0f},
		 {0.5f, 0.5f,-0.5f + zPos}, {1.0f, 0.0f, 0.0f},
		
		{-0.5f, 0.5f, 0.5f + zPos}, {0.0f, 1.0f, 0.0f}, // +Z side
		{-0.5f,-0.5f, 0.5f + zPos}, {0.0f, 1.0f, 0.0f},
		{ 0.5f, 0.5f, 0.5f + zPos}, {0.0f, 1.0f, 0.0f},
		{-0.5f,-0.5f, 0.5f + zPos}, {0.0f, 1.0f, 0.0f},
		{ 0.5f,-0.5f, 0.5f + zPos}, {0.0f, 1.0f, 0.0f},
		{ 0.5f, 0.5f, 0.5f + zPos}, {0.0f, 1.0f, 0.0f}
		};
		s_Data->indices = {
			0, 1, 2,
			0, 2, 3
		};
		vk::BufferCreateInfo vbufferinfo{};
		vbufferinfo.flags = vk::BufferCreateFlags();
		vbufferinfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
		vbufferinfo.size = 72 * sizeof(HML::Vector3<>);
		vbufferinfo.sharingMode = vk::SharingMode::eExclusive;
		s_Data->vertexBuffer = Core::Context::Get()->GetDevice()->createBuffer(vbufferinfo);

		Core::Context::Get()->allocateBufferMemory(s_Data->vertexBuffer, s_Data->veretxBufferMemory);

		void* vmemlocation = Core::Context::Get()->GetDevice()->mapMemory(s_Data->veretxBufferMemory, 0, 72 * sizeof(HML::Vector3<>));
		memcpy(vmemlocation, s_Data->verts.data(), 72 * sizeof(HML::Vector3<>));
		Core::Context::Get()->GetDevice()->unmapMemory(s_Data->veretxBufferMemory);

		s_Data->bdesc.binding = 0;
		s_Data->bdesc.stride = 2 * sizeof(HML::Vector3<>);
		s_Data->bdesc.inputRate = vk::VertexInputRate::eVertex;

		vk::VertexInputAttributeDescription attr01;
		attr01.binding = 0;
		attr01.location = 0;
		attr01.format = vk::Format::eR32G32B32Sfloat;
		attr01.offset = 0;
		s_Data->attributes.push_back(attr01);

		vk::VertexInputAttributeDescription attr02;
		attr02.binding = 0;
		attr02.location = 1;
		attr02.format = vk::Format::eR32G32B32Sfloat;
		attr02.offset = 0;
		s_Data->attributes.push_back(attr02);

		Create();
	}
	void Cube::OnUpdate()
	{
		s_Data->Model = HML::Transform::Rotate(HML::Mat4x4<>(1.0f), s_Data->rotation, HML::Vector3<>(1.0f, 1.0f, 1.0f));	//Model Matrix
		s_Data->pushConstant.ViewProjection *= s_Data->Model;																//ModelViewProjection Matrix
		vk::CommandBufferBeginInfo beginInfo{};

		s_Data->cmd[Core::Context::Get()->GetSwapchain().nextSwapchainImage].begin(beginInfo);

		vk::RenderPassBeginInfo renderPassInfo{};
		renderPassInfo.renderPass = s_Data->renderpass;
		renderPassInfo.framebuffer = s_Data->framebuffers[Core::Context::Get()->GetSwapchain().currentFrame];
		renderPassInfo.renderArea.offset.x = 0;
		renderPassInfo.renderArea.offset.y = 0;
		renderPassInfo.renderArea.extent = Core::Context::Get()->GetSwapchain().extent;

		vk::ClearValue clearColor{ std::array<float, 4>{0.2f, 0.5f, 0.7f, 1.0f} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		s_Data->cmd[Core::Context::Get()->GetSwapchain().nextSwapchainImage].beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

		s_Data->cmd[Core::Context::Get()->GetSwapchain().nextSwapchainImage].bindPipeline(vk::PipelineBindPoint::eGraphics, s_Data->graphicsPipeline);

		s_Data->cmd[Core::Context::Get()->GetSwapchain().nextSwapchainImage].bindVertexBuffers(0, s_Data->vertexBuffer, { 0 });

		s_Data->cmd[Core::Context::Get()->GetSwapchain().nextSwapchainImage].pushConstants(s_Data->pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(Data::Constant), &s_Data->pushConstant);

		s_Data->cmd[Core::Context::Get()->GetSwapchain().nextSwapchainImage].draw(36, 1, 0, 0);

		s_Data->cmd[Core::Context::Get()->GetSwapchain().nextSwapchainImage].endRenderPass();

		s_Data->cmd[Core::Context::Get()->GetSwapchain().nextSwapchainImage].end();

		vk::SubmitInfo submit_info{};
		submit_info.sType = vk::StructureType::eSubmitInfo;
		vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		submit_info.pWaitDstStageMask = &waitStage;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &Core::Context::Get()->GetImageAvailableSemaphore();
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &Core::Context::Get()->GetRenderFinishedSemaphore();
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &s_Data->cmd[Core::Context::Get()->GetSwapchain().nextSwapchainImage];

		Core::Context::Get()->GetDevice()->resetFences(Core::Context::Get()->GetInFlightFence());

		Core::Context::Get()->GetGraphicsQueue().submit(submit_info, Core::Context::Get()->GetInFlightFence());
	}
	void Cube::Shutdown()
	{
		Core::Context::Get()->GetDevice()->freeMemory(s_Data->veretxBufferMemory);
		Core::Context::Get()->GetDevice()->destroyBuffer(s_Data->vertexBuffer);
		Destroy();
		delete s_Data;
	}
	void Cube::Destroy()
	{
		Core::Context::Get()->GetDevice()->destroyPipelineLayout(s_Data->pipelineLayout);
		Core::Context::Get()->GetDevice()->destroyPipeline(s_Data->graphicsPipeline);
		for (auto framebuffer : s_Data->framebuffers)
			Core::Context::Get()->GetDevice()->destroyFramebuffer(framebuffer);
		s_Data->framebuffers.clear();
		Core::Context::Get()->GetDevice()->freeCommandBuffers(Core::Context::Get()->GetGraphicsCommandPool(), s_Data->cmd);
		s_Data->cmd.clear();
		Core::Context::Get()->GetDevice()->destroyRenderPass(s_Data->renderpass);
	}
	void Cube::Recreate()
	{
		Create();
	}
}