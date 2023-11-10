#pragma once
#include <HML/HeaderMath.hpp>
#include <API/VulkanInit.hpp>
#include "Texture.hpp"

struct aiNode;
struct aiScene;

namespace Example
{
	struct Vertex
	{
		HML::Vector3<> position;
		HML::Vector3<> normal;
		HML::Vector4<> tangent;
		HML::Vector4<> bitangent;
		HML::Vector4<> color;
		HML::Vector2<> uv;
	};
	struct AccelerationStructure
	{
		vk::AccelerationStructureKHR handle;
		uint64_t deviceAddress = 0;
		vk::Buffer buffer = VK_NULL_HANDLE;
		vk::DeviceMemory memory = VK_NULL_HANDLE;
	};
	struct Material
	{
		HML::Vector4<float> AlbedoColor;
		float Emisivity;
		float Roughness;
		float Metalic;
		int UseNormalMap;
		std::shared_ptr<Texture> Albedo;
		std::shared_ptr<Texture> Normal;
		std::shared_ptr<Texture> MetalicRoughnessMap;
		vk::Buffer UniformBuffer;
		vk::DeviceMemory Memory;
	};
	struct Mesh
	{
		Mesh(std::vector<Vertex> v, std::vector<uint32_t> i) : m_verts(v), m_indicies(i) { createBuffers(); }
		std::vector<Vertex> m_verts;
		std::vector<uint32_t> m_indicies;
		vk::Buffer m_vertexBuffer;
		vk::Buffer m_indexBuffer;
		vk::DeviceMemory m_vertexBufferMemory;
		vk::DeviceMemory m_indexBufferMemory;
		AccelerationStructure m_bottomLevelAS;
	private:
		void createBuffers();
		void createBottomLevelAccelerationStructure();
	};
	class Model
	{
	public:
		Model(const std::string& path);
		static void Init();
		void processNode(aiNode* node, const aiScene* scene);
		void DrawModel(vk::CommandBuffer& cmd, const vk::PipelineLayout& layout, const std::vector<vk::DescriptorSet>& sets);
		void Destroy();
		std::vector<Mesh> m_meshes;
		std::vector<Material> m_materials;
		static vk::DescriptorSetLayout s_MaterialDescriptorSetLayout;
		std::vector<vk::DescriptorSet> MaterialDescriptorSets;
	};
}