#pragma once
#include <API/VulkanInit.hpp>
namespace Example
{
	class Texture
	{
	public:
		Texture(const std::string& path) { load(path); }
		void Destroy();
		vk::Buffer m_buffer;
		vk::DeviceMemory m_mem;
		vk::DescriptorSetLayout m_layout;
		vk::DescriptorSet m_set;
	private:
		void load(const std::string& path);
		vk::Image m_img;
		vk::DeviceMemory m_img_mem;
		vk::ImageView m_view;
		vk::Sampler m_sampler;
	};
}