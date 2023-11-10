#pragma once
#include "VulkanInit.hpp"

namespace Core
{
	class GraphicsPipeline
	{
	public:
		GraphicsPipeline(std::string vertexShaderCode, std::string fragmentShaderCode,
			vk::VertexInputBindingDescription bdesc, std::vector<vk::VertexInputAttributeDescription> attributes, uint32_t pushConstantSize,
			std::vector<vk::DescriptorSetLayout> setLayouts, vk::RenderPass renderPass, bool pushConstantFragment = false, bool ignoreDepth = false);
		virtual ~GraphicsPipeline();
		void Destroy();

		vk::Pipeline GetPipeline() { return m_Pipeline; }
		vk::PipelineLayout GetLayout() { return m_PipelineLayout; }

	private:
		vk::Pipeline m_Pipeline;
		vk::PipelineLayout m_PipelineLayout;
	};
}