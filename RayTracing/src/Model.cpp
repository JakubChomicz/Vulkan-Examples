#include "Model.hpp"
#include <assimp/Importer.hpp>
#include <assimp/Scene.h>
#include <assimp/postprocess.h>
#include <iostream>
namespace Example
{
	static Mesh processMesh(aiMesh* mesh, const aiScene* scene)
	{
		std::vector<Vertex> _vertices;
		std::vector<uint32_t> _indices;
		//vector<Texture> textures;

		// walk through each of the mesh's vertices
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;
			// positions
			vertex.position[0] = mesh->mVertices[i].x;
			vertex.position[1] = mesh->mVertices[i].y;
			vertex.position[2] = mesh->mVertices[i].z;
			// normals
			if (mesh->HasNormals())
			{
				vertex.normal[0] = mesh->mNormals[i].x;
				vertex.normal[1] = mesh->mNormals[i].y;
				vertex.normal[2] = mesh->mNormals[i].z;
			}
			// texture coordinates
			if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
			{
				vertex.uv[0] = mesh->mTextureCoords[0][i].x;
				vertex.uv[1] = mesh->mTextureCoords[0][i].y;
				// tangent
				vertex.tangent[0] = mesh->mTangents[i].x;
				vertex.tangent[1] = mesh->mTangents[i].y;
				vertex.tangent[2] = mesh->mTangents[i].z;
				vertex.tangent[3] = 0.0f;
				// bitangent
				vertex.bitangent[0] = mesh->mBitangents[i].x;
				vertex.bitangent[1] = mesh->mBitangents[i].y;
				vertex.bitangent[2] = mesh->mBitangents[i].z;
				vertex.bitangent[3] = 0.0f;
			}
			else
			{
				vertex.uv[0] = 0.0f;
				vertex.uv[1] = 0.0f;
			}

			if (mesh->HasVertexColors(0))
			{
				vertex.color[0] = mesh->mColors[0][i].r;
				vertex.color[1] = mesh->mColors[0][i].g;
				vertex.color[2] = mesh->mColors[0][i].b;
				vertex.color[3] = mesh->mColors[0][i].a;
			}
			else
			{
				vertex.color[0] = 1;
				vertex.color[1] = 1;
				vertex.color[2] = 1;
				vertex.color[3] = 1;
			}

			_vertices.push_back(vertex);
		}
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];

			for (unsigned int j = 0; j < face.mNumIndices; j++)
				_indices.push_back(face.mIndices[j]);
		}

		return Mesh(_vertices, _indices);
	}
	void Model::processNode(aiNode* node, const aiScene* scene)
	{
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			std::string name = scene->mMeshes[node->mMeshes[i]]->mName.data;
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			m_meshes.push_back(processMesh(mesh, scene));
		}
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			processNode(node->mChildren[i], scene);
		}
	}
	void Model::DrawModel(vk::CommandBuffer& cmd)
	{
		for (auto& mesh : m_meshes)
		{
			cmd.bindVertexBuffers(0, mesh.m_vertexBuffer, { 0 });
			cmd.bindIndexBuffer(mesh.m_indexBuffer, 0, vk::IndexType::eUint32);
			cmd.drawIndexed(mesh.m_indicies.size(), 1, 0, 0/*fix*/, 0);
		}
	}
	void Model::Destroy()
	{
		for (auto& mesh : m_meshes)
		{
			Core::Context::_device->freeMemory(mesh.m_vertexBufferMemory);
			Core::Context::_device->freeMemory(mesh.m_indexBufferMemory);
			Core::Context::_device->destroyBuffer(mesh.m_vertexBuffer);
			Core::Context::_device->destroyBuffer(mesh.m_indexBuffer);
			mesh.m_verts.clear();
			mesh.m_indicies.clear();
		}
	}
	Model::Model(const std::string& path)
	{
		Assimp::Importer imp;
		const auto* scene = imp.ReadFile(path,
			aiProcessPreset_TargetRealtime_Quality |
			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_FlipUVs |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_FindDegenerates |
			aiProcess_GenUVCoords |
			aiProcess_SortByPType);
		if (!scene)
		{
			std::cout << "invalid file for assimp" << std::endl;
			return;
		}
		processNode(scene->mRootNode, scene);
	}
	void Mesh::createBuffers()
	{
		vk::BufferCreateInfo vbufferinfo{};
		vbufferinfo.flags = vk::BufferCreateFlags();
		vbufferinfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
		vbufferinfo.size = m_verts.size() * sizeof(Vertex);
		vbufferinfo.sharingMode = vk::SharingMode::eExclusive;
		m_vertexBuffer = Core::Context::_device->createBuffer(vbufferinfo);

		Core::Context::allocateBufferMemory(m_vertexBuffer, m_vertexBufferMemory, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::MemoryAllocateFlagBits::eDeviceAddress);

		void* vmemlocation = Core::Context::_device->mapMemory(m_vertexBufferMemory, 0, m_verts.size() * sizeof(Vertex));
		memcpy(vmemlocation, m_verts.data(), m_verts.size() * sizeof(Vertex));
		Core::Context::_device->unmapMemory(m_vertexBufferMemory);

		vk::BufferCreateInfo ibufferinfo{};
		ibufferinfo.flags = vk::BufferCreateFlags();
		ibufferinfo.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
		ibufferinfo.size = m_indicies.size() * sizeof(uint32_t);
		ibufferinfo.sharingMode = vk::SharingMode::eExclusive;
		m_indexBuffer = Core::Context::_device->createBuffer(ibufferinfo);

		Core::Context::allocateBufferMemory(m_indexBuffer, m_indexBufferMemory, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::MemoryAllocateFlagBits::eDeviceAddress);

		void* imemlocation = Core::Context::_device->mapMemory(m_indexBufferMemory, 0, m_indicies.size() * sizeof(uint32_t));
		memcpy(imemlocation, m_indicies.data(), m_indicies.size() * sizeof(uint32_t));
		Core::Context::_device->unmapMemory(m_indexBufferMemory);

		createBottomLevelAccelerationStructure();
	}
	void Mesh::createBottomLevelAccelerationStructure()
	{
		vk::BufferDeviceAddressInfo info1;
		info1.buffer = m_vertexBuffer;
		vk::BufferDeviceAddressInfo info2;
		info2.buffer = m_indexBuffer;
		vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress;
		vertexBufferDeviceAddress.deviceAddress = Core::Context::_device->getBufferAddressKHR(info1);
		vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress;
		indexBufferDeviceAddress.deviceAddress = Core::Context::_device->getBufferAddressKHR(info2);

		uint32_t numTriangles = static_cast<uint32_t>(m_indicies.size()) / 3;

		// Build
		vk::AccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
		accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eTriangles;
		accelerationStructureGeometry.geometry.triangles.sType = vk::StructureType::eAccelerationStructureGeometryTrianglesDataKHR;
		accelerationStructureGeometry.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
		accelerationStructureGeometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
		accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Vertex);		//Might need fix
		accelerationStructureGeometry.geometry.triangles.maxVertex = static_cast<uint32_t>(m_verts.size());
		accelerationStructureGeometry.geometry.triangles.indexType = vk::IndexType::eUint32;
		accelerationStructureGeometry.geometry.triangles.indexData = indexBufferDeviceAddress;
		accelerationStructureGeometry.geometry.triangles.transformData.deviceAddress = 0;
		accelerationStructureGeometry.geometry.triangles.transformData.hostAddress = nullptr;

		// Get size info
		vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
		accelerationStructureBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		vk::AccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		Core::Context::_device->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &accelerationStructureBuildGeometryInfo, &numTriangles, &accelerationStructureBuildSizesInfo);

		vk::BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		bufferCreateInfo.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
		m_bottomLevelAS.buffer = Core::Context::_device->createBuffer(bufferCreateInfo);

		vk::MemoryRequirements memoryRequirements = Core::Context::_device->getBufferMemoryRequirements(m_bottomLevelAS.buffer);
		vk::MemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
		memoryAllocateFlagsInfo.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
		vk::MemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = Core::Context::findMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		Core::Context::_device->allocateMemory(&memoryAllocateInfo, nullptr, &m_bottomLevelAS.memory);
		Core::Context::_device->bindBufferMemory(m_bottomLevelAS.buffer, m_bottomLevelAS.memory, 0);
		// Acceleration structure
		vk::AccelerationStructureCreateInfoKHR accelerationStructureCreate_info{};
		accelerationStructureCreate_info.buffer = m_bottomLevelAS.buffer;
		accelerationStructureCreate_info.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreate_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;

		m_bottomLevelAS.handle = Core::Context::_device->createAccelerationStructureKHR(accelerationStructureCreate_info);

		// AS device address
		vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.accelerationStructure = m_bottomLevelAS.handle;
		m_bottomLevelAS.deviceAddress = Core::Context::_device->getAccelerationStructureAddressKHR(accelerationDeviceAddressInfo);


		// Create a small scratch buffer used during build of the bottom level acceleration structure
		Core::ScratchBuffer scratchBuffer = Core::createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		vk::AccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
		accelerationBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
		accelerationBuildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
		accelerationBuildGeometryInfo.dstAccelerationStructure = m_bottomLevelAS.handle;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

		// Build the acceleration structure on the device via a one-time command buffer submission
		// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
		vk::CommandBufferAllocateInfo CMDallocInfo{};
		CMDallocInfo.level = vk::CommandBufferLevel::ePrimary;
		CMDallocInfo.commandPool = Core::Context::_graphicsCommandPool;
		CMDallocInfo.commandBufferCount = 1;
		vk::CommandBuffer commandBuffer = Core::Context::_device->allocateCommandBuffers(CMDallocInfo)[0];
		vk::CommandBufferBeginInfo beg{};
		beg.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
		commandBuffer.begin(beg);
		commandBuffer.buildAccelerationStructuresKHR(accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos);

		commandBuffer.end();

		vk::SubmitInfo submitInfo{};
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		// Create fence to ensure that the command buffer has finished executing
		vk::FenceCreateInfo fenceInfo{};
		vk::Fence fence = Core::Context::_device->createFence(fenceInfo);
		// Submit to the queue
		Core::Context::_graphicsQueue.submit(submitInfo, fence);
		// Wait for the fence to signal that command buffer has finished executing
		Core::Context::_device->waitForFences(fence, true, UINT64_MAX);
		Core::Context::_device->destroyFence(fence);
		Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, commandBuffer);

		Core::deleteScratchBuffer(scratchBuffer);
	}
}