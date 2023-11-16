#include "Model.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>
#include <iostream>
#include <filesystem>

#define AI_MATKEY_ROUGHNESS_FACTOR "$mat.roughnessFactor", 0, 0

namespace Example
{
	vk::DescriptorSetLayout Model::s_MaterialDescriptorSetLayout;

	void Model::Init()
	{
		std::vector<vk::DescriptorSetLayoutBinding> bindings;

		vk::DescriptorSetLayoutBinding binding;
		binding.binding = 0;
		binding.descriptorType = vk::DescriptorType::eUniformBuffer;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		bindings.push_back(binding);

		binding.binding = 1;
		binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		bindings.push_back(binding);

		binding.binding = 2;
		binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		bindings.push_back(binding);

		binding.binding = 3;
		binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		bindings.push_back(binding);

		binding.binding = 4;
		binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		bindings.push_back(binding);

		vk::DescriptorSetLayoutCreateInfo layoutinfo;
		layoutinfo.bindingCount = bindings.size();
		layoutinfo.pBindings = bindings.data();

		s_MaterialDescriptorSetLayout = Core::Context::Get()->GetDevice()->createDescriptorSetLayout(layoutinfo);
	}

	struct BufferData
	{
		HML::Vector4<float> AlbedoColor;
		float Emisivity;
		float Roughness;
		float Metalic;
		int UseMetalicMap;
	};
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
	void Model::DrawModel(vk::CommandBuffer& cmd, const vk::PipelineLayout& layout, const std::vector<vk::DescriptorSet>& sets)
	{
		for (uint32_t i = 0; i < m_meshes.size(); i++)
		{
			auto& mesh = m_meshes[i];
			std::vector<vk::DescriptorSet> allSets = sets;
			allSets.push_back(MaterialDescriptorSets[i]);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, allSets, {  });
			cmd.bindVertexBuffers(0, mesh.m_vertexBuffer, { 0 });
			cmd.bindIndexBuffer(mesh.m_indexBuffer, 0, vk::IndexType::eUint32);
			cmd.drawIndexed(mesh.m_indicies.size(), 1, 0, 0, 0);
		}
	}
	void Model::Destroy()
	{
		for (auto& mesh : m_meshes)
		{
			Core::Context::Get()->GetDevice()->freeMemory(mesh.m_vertexBufferMemory);
			Core::Context::Get()->GetDevice()->freeMemory(mesh.m_indexBufferMemory);
			Core::Context::Get()->GetDevice()->destroyBuffer(mesh.m_vertexBuffer);
			Core::Context::Get()->GetDevice()->destroyBuffer(mesh.m_indexBuffer);
			mesh.m_verts.clear();
			mesh.m_indicies.clear();
		}
	}
	Model::Model(const std::string& path)
	{
		std::string directory = path.substr(0, path.find_last_of('/'));
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
		if (scene->HasMaterials())
		{
			uint32_t materialCount = scene->mNumMaterials;
			m_materials.resize(materialCount);
			for (uint32_t i = 0; i < materialCount; i++)
			{
				auto aiMat = scene->mMaterials[i];
				HML::Vector4<float> albedo{ 1.0f, 1.0f, 1.0f, 1.0f };
				float roughness, metalic, emission = 0.0f, normalIntensity = 1.0f;
				aiColor4D aiColorRGBA;
				aiColor3D aiColorRGB, aiEmission;
				if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, aiColorRGBA) == aiReturn_SUCCESS)
					albedo = { aiColorRGBA.r, aiColorRGBA.g, aiColorRGBA.b, aiColorRGBA.a };
				else if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, aiColorRGB) == aiReturn_SUCCESS)
					albedo = { aiColorRGB.r, aiColorRGB.g, aiColorRGB.b, 1.0f };

				if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, aiEmission) == AI_SUCCESS)
					emission = aiEmission.r;

				if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != aiReturn_SUCCESS)		//FIX AI_MATKEY_ROUGHNESS_FACTOR
					roughness = 0.5f;

				if (aiMat->Get(AI_MATKEY_REFLECTIVITY, metalic) != aiReturn_SUCCESS)
					metalic = 0.0f;

				m_materials[i].AlbedoColor = albedo;
				m_materials[i].Emisivity = emission;
				m_materials[i].Roughness = roughness;
				m_materials[i].Metalic = metalic;

				aiString aiTexPathAlbedoMap;
				aiString aiTexPathNormalMap;
				aiString aiTexPathRoughnessMap;

				bool hasAlbedoMap = aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPathAlbedoMap) == aiReturn_SUCCESS;
				bool hasNormalMap = aiMat->GetTexture(aiTextureType_NORMALS, 0, &aiTexPathNormalMap) == aiReturn_SUCCESS;
				bool hasMetalicRoughnessMap = aiMat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &aiTexPathRoughnessMap) == aiReturn_SUCCESS;
				bool metalicTextureFound = false;

				if (hasAlbedoMap)
				{
					std::filesystem::path dir = directory;
					dir /= aiTexPathAlbedoMap.C_Str();
					m_materials[i].Albedo = std::make_shared<Texture>(dir.string(), false);
				}
				else
				{
					m_materials[i].Albedo = Texture::s_EmptyTexture;
				}

				if (hasNormalMap)
				{
					std::filesystem::path dir = directory;
					dir /= aiTexPathNormalMap.C_Str();
					m_materials[i].Normal = std::make_shared<Texture>(dir.string(), false);
					m_materials[i].UseNormalMap = true;
				}
				else
				{
					m_materials[i].UseNormalMap = false;
					m_materials[i].Normal = Texture::s_EmptyTexture;
				}

				if (hasMetalicRoughnessMap)
				{
					std::filesystem::path dir = directory;
					dir /= aiTexPathRoughnessMap.C_Str();
					m_materials[i].MetalicRoughnessMap = std::make_shared<Texture>(dir.string(), false);
				}
				else
				{
					m_materials[i].MetalicRoughnessMap = Texture::s_EmptyTexture;
				}

				vk::BufferCreateInfo bufferinfo{};
				bufferinfo.flags = vk::BufferCreateFlags();
				bufferinfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
				bufferinfo.size = sizeof(BufferData);
				bufferinfo.sharingMode = vk::SharingMode::eExclusive;
				m_materials[i].UniformBuffer = Core::Context::Get()->GetDevice()->createBuffer(bufferinfo);

				Core::Context::Get()->allocateBufferMemory(m_materials[i].UniformBuffer, m_materials[i].Memory);

				BufferData temp;
				temp.AlbedoColor = m_materials[i].AlbedoColor;
				temp.Emisivity = m_materials[i].Emisivity;
				temp.Roughness = m_materials[i].Roughness;
				temp.Metalic = m_materials[i].Metalic;
				temp.UseMetalicMap = m_materials[i].UseNormalMap;

				void* vmemlocation = Core::Context::Get()->GetDevice()->mapMemory(m_materials[i].Memory, 0, sizeof(BufferData));
				memcpy(vmemlocation, &temp, sizeof(BufferData));
				Core::Context::Get()->GetDevice()->unmapMemory(m_materials[i].Memory);
			}
		}
		MaterialDescriptorSets.resize(m_materials.size());
		for (uint32_t i = 0; i < static_cast<uint32_t>(MaterialDescriptorSets.size()); i++)
		{
			auto& set = MaterialDescriptorSets[i];
			auto& material = m_materials[i];

			vk::DescriptorSetAllocateInfo info;
			info.sType = vk::StructureType::eDescriptorSetAllocateInfo;
			info.descriptorPool = Core::Context::Get()->GetDescriptorPool();
			info.descriptorSetCount = 1;
			info.pSetLayouts = &s_MaterialDescriptorSetLayout;

			MaterialDescriptorSets[i] = Core::Context::Get()->GetDevice()->allocateDescriptorSets(info).front();

			vk::DescriptorBufferInfo bufferinfo;
			bufferinfo.buffer = material.UniformBuffer;
			bufferinfo.offset = 0;
			bufferinfo.range = 8 * sizeof(float);
			vk::WriteDescriptorSet write;
			write.sType = vk::StructureType::eWriteDescriptorSet;
			write.dstSet = set;
			write.dstBinding = 0;
			write.descriptorType = vk::DescriptorType::eUniformBuffer;
			write.pBufferInfo = &bufferinfo;
			write.descriptorCount = 1;

			Core::Context::Get()->GetDevice()->updateDescriptorSets(write, nullptr);

			std::shared_ptr<Texture> tex;

			for (uint32_t i = 1; i < 4; i++)
			{
				switch (i)
				{
				case 1: tex = material.Albedo;			break;
				case 2: tex = material.Normal;			break;
				case 3: tex = material.MetalicRoughnessMap;	break;
				default: break;
				}
				vk::DescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				imageInfo.imageView = tex->GetImageView();
				imageInfo.sampler = tex->GetSampler();

				vk::WriteDescriptorSet write2;

				write2.dstSet = set;
				write2.dstBinding = i;
				write2.dstArrayElement = 0;
				write2.descriptorType = vk::DescriptorType::eCombinedImageSampler;
				write2.descriptorCount = 1;
				write2.pImageInfo = &imageInfo;

				Core::Context::Get()->GetDevice()->updateDescriptorSets(write2, nullptr);
			}

		}
	}
	void Mesh::createBuffers()
	{
		vk::BufferCreateInfo vbufferinfo{};
		vbufferinfo.flags = vk::BufferCreateFlags();
		vbufferinfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
		vbufferinfo.size = m_verts.size() * sizeof(Vertex);
		vbufferinfo.sharingMode = vk::SharingMode::eExclusive;
		m_vertexBuffer = Core::Context::Get()->GetDevice()->createBuffer(vbufferinfo);

		Core::Context::Get()->allocateBufferMemory(m_vertexBuffer, m_vertexBufferMemory, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::MemoryAllocateFlagBits::eDeviceAddress);

		void* vmemlocation = Core::Context::Get()->GetDevice()->mapMemory(m_vertexBufferMemory, 0, m_verts.size() * sizeof(Vertex));
		memcpy(vmemlocation, m_verts.data(), m_verts.size() * sizeof(Vertex));
		Core::Context::Get()->GetDevice()->unmapMemory(m_vertexBufferMemory);

		vk::BufferCreateInfo ibufferinfo{};
		ibufferinfo.flags = vk::BufferCreateFlags();
		ibufferinfo.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
		ibufferinfo.size = m_indicies.size() * sizeof(uint32_t);
		ibufferinfo.sharingMode = vk::SharingMode::eExclusive;
		m_indexBuffer = Core::Context::Get()->GetDevice()->createBuffer(ibufferinfo);

		Core::Context::Get()->allocateBufferMemory(m_indexBuffer, m_indexBufferMemory, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::MemoryAllocateFlagBits::eDeviceAddress);

		void* imemlocation = Core::Context::Get()->GetDevice()->mapMemory(m_indexBufferMemory, 0, m_indicies.size() * sizeof(uint32_t));
		memcpy(imemlocation, m_indicies.data(), m_indicies.size() * sizeof(uint32_t));
		Core::Context::Get()->GetDevice()->unmapMemory(m_indexBufferMemory);

		createBottomLevelAccelerationStructure();
	}
	void Mesh::createBottomLevelAccelerationStructure()
	{
		vk::BufferDeviceAddressInfo info1;
		info1.buffer = m_vertexBuffer;
		vk::BufferDeviceAddressInfo info2;
		info2.buffer = m_indexBuffer;
		vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress;
		vertexBufferDeviceAddress.deviceAddress = Core::Context::Get()->GetDevice()->getBufferAddressKHR(info1);
		vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress;
		indexBufferDeviceAddress.deviceAddress = Core::Context::Get()->GetDevice()->getBufferAddressKHR(info2);

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
		Core::Context::Get()->GetDevice()->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &accelerationStructureBuildGeometryInfo, &numTriangles, &accelerationStructureBuildSizesInfo);

		vk::BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		bufferCreateInfo.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
		m_bottomLevelAS.buffer = Core::Context::Get()->GetDevice()->createBuffer(bufferCreateInfo);

		vk::MemoryRequirements memoryRequirements = Core::Context::Get()->GetDevice()->getBufferMemoryRequirements(m_bottomLevelAS.buffer);
		vk::MemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
		memoryAllocateFlagsInfo.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
		vk::MemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = Core::Context::Get()->findMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		Core::Context::Get()->GetDevice()->allocateMemory(&memoryAllocateInfo, nullptr, &m_bottomLevelAS.memory);
		Core::Context::Get()->GetDevice()->bindBufferMemory(m_bottomLevelAS.buffer, m_bottomLevelAS.memory, 0);
		// Acceleration structure
		vk::AccelerationStructureCreateInfoKHR accelerationStructureCreate_info{};
		accelerationStructureCreate_info.buffer = m_bottomLevelAS.buffer;
		accelerationStructureCreate_info.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreate_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;

		m_bottomLevelAS.handle = Core::Context::Get()->GetDevice()->createAccelerationStructureKHR(accelerationStructureCreate_info);

		// AS device address
		vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.accelerationStructure = m_bottomLevelAS.handle;
		m_bottomLevelAS.deviceAddress = Core::Context::Get()->GetDevice()->getAccelerationStructureAddressKHR(accelerationDeviceAddressInfo);


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
		CMDallocInfo.commandPool = Core::Context::Get()->GetGraphicsCommandPool();
		CMDallocInfo.commandBufferCount = 1;
		vk::CommandBuffer commandBuffer = Core::Context::Get()->GetDevice()->allocateCommandBuffers(CMDallocInfo)[0];
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
		vk::Fence fence = Core::Context::Get()->GetDevice()->createFence(fenceInfo);
		// Submit to the queue
		Core::Context::Get()->GetGraphicsQueue().submit(submitInfo, fence);
		// Wait for the fence to signal that command buffer has finished executing
		Core::Context::Get()->GetDevice()->waitForFences(fence, true, UINT64_MAX);
		Core::Context::Get()->GetDevice()->destroyFence(fence);
		Core::Context::Get()->GetDevice()->freeCommandBuffers(Core::Context::Get()->GetGraphicsCommandPool(), commandBuffer);

		Core::deleteScratchBuffer(scratchBuffer);
	}
}