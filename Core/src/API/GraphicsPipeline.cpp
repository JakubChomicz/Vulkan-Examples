#include "GraphicsPipeline.h"
#include <string>
#include <fstream>
#include <iostream>

namespace Core
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
	GraphicsPipeline::GraphicsPipeline(std::string vertexShaderCode, std::string fragmentShaderCode,
		vk::VertexInputBindingDescription bdesc, std::vector<vk::VertexInputAttributeDescription> attributes, uint32_t pushConstantSize,
		std::vector<vk::DescriptorSetLayout> setLayouts, vk::RenderPass renderPass, bool pushConstantFragment, bool ignoreDepth)
	{
		//Graphics Pipeline
		{
			std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

			vk::PushConstantRange range;
			range.offset = 0;
			range.size = pushConstantSize;
			range.stageFlags = pushConstantFragment ? vk::ShaderStageFlagBits::eFragment : vk::ShaderStageFlagBits::eVertex;

			vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
			vertexInputInfo.flags = vk::PipelineVertexInputStateCreateFlags();
			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.pVertexBindingDescriptions = &bdesc;
			vertexInputInfo.vertexAttributeDescriptionCount = attributes.size();
			vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

			vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
			inputAssemblyInfo.flags = vk::PipelineInputAssemblyStateCreateFlags();
			inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;

#ifdef M_DEBUG
			std::cout << "Create vertex shader module" << std::endl;
#endif
			vk::ShaderModule vertexShader = createShaderModule(vertexShaderCode);
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
			vk::ShaderModule fragmentShader = createShaderModule(fragmentShaderCode);
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

			vk::PipelineDepthStencilStateCreateInfo depthStencilInfo;
			depthStencilInfo.sType = vk::StructureType::ePipelineDepthStencilStateCreateInfo;
			depthStencilInfo.depthTestEnable = ignoreDepth ? false : true;
			depthStencilInfo.depthWriteEnable = ignoreDepth ? false : true;
			depthStencilInfo.depthCompareOp = vk::CompareOp::eLess;
			depthStencilInfo.depthBoundsTestEnable = false;
			depthStencilInfo.stencilTestEnable = false;

			//Pipeline Layout

			vk::PipelineLayoutCreateInfo layoutInfo;
			layoutInfo.flags = vk::PipelineLayoutCreateFlags();
			layoutInfo.setLayoutCount = setLayouts.size();
			layoutInfo.pSetLayouts = setLayouts.data();
			if (pushConstantSize <= 0)
			{
				layoutInfo.pushConstantRangeCount = 0;
				layoutInfo.pPushConstantRanges = nullptr;
			}
			else
			{
				layoutInfo.pushConstantRangeCount = 1;
				layoutInfo.pPushConstantRanges = &range;
			}

			m_PipelineLayout = Core::Context::_device->createPipelineLayout(layoutInfo);

			//Extra stuff
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
			pipelineInfo.layout = m_PipelineLayout;
			pipelineInfo.renderPass = renderPass;
			pipelineInfo.subpass = 0;
			pipelineInfo.basePipelineHandle = nullptr;

			m_Pipeline = Core::Context::_device->createGraphicsPipeline(nullptr, pipelineInfo).value;


			Core::Context::_device->destroyShaderModule(vertexShader);
			Core::Context::_device->destroyShaderModule(fragmentShader);
		}
	}

	GraphicsPipeline::~GraphicsPipeline()
	{
	}

	void GraphicsPipeline::Destroy()
	{
		Core::Context::_device->destroyPipelineLayout(m_PipelineLayout);
		Core::Context::_device->destroyPipeline(m_Pipeline);
	}

}
