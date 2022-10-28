#pragma once
#include <HML/HeaderMath.hpp>
#include <API/VulkanInit.hpp>

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
	struct Mesh
	{
		Mesh(std::vector<Vertex> v, std::vector<uint32_t> i) : m_verts(v), m_indicies(i) { createBuffers(); }
		std::vector<Vertex> m_verts;
		std::vector<uint32_t> m_indicies;
		vk::Buffer m_vertexBuffer;
		vk::Buffer m_indexBuffer;
		vk::DeviceMemory m_vertexBufferMemory;
		vk::DeviceMemory m_indexBufferMemory;
	private:
		void createBuffers();
	};
	class Model
	{
	public:
		Model(const std::string& path);
		void processNode(aiNode* node, const aiScene* scene);
		void DrawModel(vk::CommandBuffer& cmd);
		void Destroy();
		std::vector<Mesh> m_meshes;
	};
}