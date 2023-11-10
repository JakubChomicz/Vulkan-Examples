#pragma once
#include <API/VulkanInit.hpp>
namespace Example
{
	class Texture
	{
	public:
		Texture(const std::string& path, bool createDescriptor = true) : m_createDescriptor(createDescriptor) { load(path); }
		void Destroy();

		const vk::ImageView& GetImageView() { return m_view; }
		const vk::Sampler& GetSampler() { return m_sampler; }

		vk::Buffer m_buffer;
		vk::DeviceMemory m_mem;
		vk::DescriptorSetLayout m_layout;
		vk::DescriptorSet m_set;
		static void Init();
		static std::shared_ptr<Texture> s_EmptyTexture;
	private:
		void load(const std::string& path);
		vk::Image m_img;
		vk::DeviceMemory m_img_mem;
		vk::ImageView m_view;
		vk::Sampler m_sampler;
		bool m_createDescriptor;
	};
}