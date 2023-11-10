#pragma once
#include <API/VulkanInit.hpp>

namespace Example
{
	struct EnvMapData
	{
		vk::Image image;
		vk::DeviceMemory memory;
		vk::ImageView view;
		vk::Sampler sampler;
	};
	class EnvironmentMap
	{
	public:
		EnvironmentMap(const std::string& path) { load(path); }
		void Destroy();

		EnvMapData GetRadianceMap() { return m_radiance; }
		EnvMapData GetIrradianceMap() { return m_irradiance; }
	private:
		void load(const std::string& path);
		EnvMapData m_unfiltered;
		EnvMapData m_radiance;
		EnvMapData m_irradiance;
		bool m_createDescriptor;
	};
}