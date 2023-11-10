#include "RayTracing.hpp"
#include <HML/HeaderMath.hpp>
#include "API/VulkanInit.hpp"
#include "App.hpp"
#include <string>
#include <fstream>
#include "Model.hpp"
#include "Camera.hpp"
#include "Texture.hpp"

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

	template <class integral>
	constexpr bool is_aligned(integral x, size_t a) noexcept
	{
		return (x & (integral(a) - 1)) == 0;
	}

	template <class integral>
	constexpr integral align_up(integral x, size_t a) noexcept
	{
		return integral((x + (integral(a) - 1)) & ~integral(a - 1));
	}

	template <class integral>
	constexpr integral align_down(integral x, size_t a) noexcept
	{
		return integral(x & ~integral(a - 1));
	}

	static vk::ShaderModule createShaderModule(std::string filename) {

		std::vector<char> sourceCode = readFile(filename);
		vk::ShaderModuleCreateInfo moduleInfo = {};
		moduleInfo.flags = vk::ShaderModuleCreateFlags();
		moduleInfo.codeSize = sourceCode.size();
		moduleInfo.pCode = reinterpret_cast<const uint32_t*>(sourceCode.data());
		return Core::Context::_device->createShaderModule(moduleInfo);
	}
	struct RayPushConstant
	{
		HML::Vector4<>  clearColor;
		HML::Vector3<>  lightPosition;
		float lightIntensity;
		int   lightType;
	};
	class ShaderBindingTable 
	{
	public:
		ShaderBindingTable(uint32_t handleCount)
		{
			vk::PhysicalDeviceRayTracingPipelinePropertiesKHR prop{};
			vk::PhysicalDeviceProperties2 prop2{};
			prop2.pNext = &prop;
			Core::Context::_GPU.getProperties2(&prop2);

			vk::BufferCreateInfo bufInfo;
			bufInfo.usage = vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
			bufInfo.size = prop.shaderGroupHandleSize * handleCount;
			bufInfo.sharingMode = vk::SharingMode::eExclusive;
			buffer = Core::Context::_device->createBuffer(bufInfo);

			size = prop.shaderGroupHandleSize * handleCount;

			vk::MemoryRequirements memoryRequirements = Core::Context::_device->getBufferMemoryRequirements(buffer);

			vk::MemoryAllocateInfo allocInfo{};
			allocInfo.allocationSize = prop.shaderGroupHandleSize * handleCount;
			allocInfo.memoryTypeIndex = Core::Context::findMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

			mem = Core::Context::_device->allocateMemory(allocInfo);

			vk::BufferDeviceAddressInfo addressinfo;
			addressinfo.buffer = buffer;

			//const uint32_t handleSizeAligned = vks::tools::alignedSize(rayTracingPipelineProperties.shaderGroupHandleSize, rayTracingPipelineProperties.shaderGroupHandleAlignment);	//Might need fix
			vk::StridedDeviceAddressRegionKHR stridedDeviceAddressRegionKHR{};
			stridedDeviceAddressRegionKHR.deviceAddress = Core::Context::_device->getBufferAddressKHR(addressinfo);
			stridedDeviceAddressRegionKHR.stride = prop.shaderGroupHandleSize;																											//Might need fix handleSizeAligned HERE
			stridedDeviceAddressRegionKHR.size = handleCount * prop.shaderGroupHandleSize;																								//Might need fix handleCount * handleSizeAligned HERE

			stridedDeviceAddressRegion = stridedDeviceAddressRegionKHR;

			Core::Context::_device->mapMemory(mem, 0, size);	//DELETE MAYBE
		}
		uint32_t size{};
		vk::Buffer buffer{};
		vk::DeviceMemory mem{};
		vk::StridedDeviceAddressRegionKHR stridedDeviceAddressRegion{};
	};
	struct Data
	{
		RayPushConstant Ray{};
		struct DepthAttachment
		{
			vk::Image image;
			vk::ImageView view;
			vk::MemoryRequirements MemRequs;
			vk::DeviceMemory Memory;
		};
		struct Constant
		{
			HML::Mat4x4<> ModelTransform;
		};
		HML::Mat4x4<> ShadowTransform = HML::Mat4x4<>(1.0f);
		vk::VertexInputBindingDescription bdesc;
		std::vector<vk::VertexInputAttributeDescription> attributes;
		vk::PipelineLayout pipelineLayout;
		vk::PipelineLayout RTpipelineLayout;
		vk::Pipeline RTPipeline;
		vk::Pipeline graphicsPipeline;
		vk::RenderPass renderpass;
		std::vector<vk::Framebuffer> framebuffers;
		std::vector<vk::CommandBuffer> cmd;
		std::string vertex = "shaders/vertex.spv";
		std::string fragment = "shaders/fragment.spv";
		//World Data
		HML::Vector3<> Position = {0.0f, 0.0f, 5.0f};
		Constant pushConstant;
		float rotation = 0.01f;
		std::shared_ptr<Model> bunny;
		std::shared_ptr<Model> plane;
		std::vector<DepthAttachment> depth_attachment;
		std::shared_ptr<Camera> cam;
		vk::DescriptorSetLayout CamDescriptorSetLayout;
		vk::DescriptorSet CamDescriptorSet;

		std::shared_ptr<ShaderBindingTable> raygen;
		std::shared_ptr<ShaderBindingTable> miss;
		std::shared_ptr<ShaderBindingTable> hit;

		vk::StridedDeviceAddressRegionKHR m_rgenRegion{};
		vk::StridedDeviceAddressRegionKHR m_missRegion{};
		vk::StridedDeviceAddressRegionKHR m_hitRegion{};
		vk::StridedDeviceAddressRegionKHR m_callRegion{};

		AccelerationStructure TopLevelAccelerationStructure;
		vk::DescriptorSetLayout RT_SetLayout;
		vk::DescriptorSet Rt_Set;
		struct StorageImage
		{
			vk::Image handle;
			vk::ImageView view_handle;
			vk::DeviceMemory mem;
		};
		struct ShaderBindingTableBufferData
		{
			vk::Buffer buffer;
			vk::DeviceMemory mem;
			vk::DeviceSize size;
		};
		ShaderBindingTableBufferData RTShaderBindingTableBuffer;
		StorageImage storageImg;
		std::vector<vk::RayTracingShaderGroupCreateInfoKHR> rtShaderGroups;

		bool needsResize = true;
	};
	static Data* s_Data = new Data();

	void CreateStorageImage(vk::Format format)
	{
		vk::ImageCreateInfo image{};
		image.imageType = vk::ImageType::e2D;
		image.format = format;
		image.extent.width = Core::Context::_swapchain.extent.width;
		image.extent.height = Core::Context::_swapchain.extent.height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = 1;
		image.samples = vk::SampleCountFlagBits::e1;
		image.tiling = vk::ImageTiling::eOptimal;
		image.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;
		image.initialLayout = vk::ImageLayout::eUndefined;
		s_Data->storageImg.handle = Core::Context::_device->createImage(image);

		vk::MemoryRequirements memReqs;
		memReqs = Core::Context::_device->getImageMemoryRequirements(s_Data->storageImg.handle);
		vk::MemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.allocationSize = memReqs.size;
		memoryAllocateInfo.memoryTypeIndex = Core::Context::findMemoryType(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		s_Data->storageImg.mem = Core::Context::_device->allocateMemory(memoryAllocateInfo);
		Core::Context::_device->bindImageMemory(s_Data->storageImg.handle, s_Data->storageImg.mem, 0);

		vk::ImageSubresourceRange range{};
		range.aspectMask = vk::ImageAspectFlagBits::eColor;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		vk::ImageViewCreateInfo colorImageView{};
		colorImageView.viewType = vk::ImageViewType::e2D;
		colorImageView.format = format;
		colorImageView.subresourceRange = range;
		colorImageView.image = s_Data->storageImg.handle;
		s_Data->storageImg.view_handle = Core::Context::_device->createImageView(colorImageView);

		vk::CommandBufferAllocateInfo inf;
		inf.level = vk::CommandBufferLevel::ePrimary;
		inf.commandBufferCount = 1;
		inf.commandPool = Core::Context::_graphicsCommandPool;

		vk::CommandBuffer cmdBuffer = Core::Context::_device->allocateCommandBuffers(inf)[0];
		vk::CommandBufferBeginInfo beginfo{};
		beginfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
		cmdBuffer.begin(beginfo);
		
		vk::ImageMemoryBarrier barrier;

		barrier.oldLayout = vk::ImageLayout::eUndefined;
		barrier.newLayout = vk::ImageLayout::eGeneral;
		barrier.image = s_Data->storageImg.handle;
		barrier.subresourceRange = range;
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		//barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;		//For some reason this should be ok

		cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), nullptr, nullptr, barrier);

		cmdBuffer.end();
		vk::SubmitInfo sub;
		sub.commandBufferCount = 1;
		sub.pCommandBuffers = &cmdBuffer;
		sub.pSignalSemaphores = nullptr;
		sub.signalSemaphoreCount = 0;
		sub.pWaitSemaphores = nullptr;
		sub.waitSemaphoreCount = 0;
		Core::Context::_graphicsQueue.submit(sub);
		Core::Context::_device->waitIdle();
		Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, cmdBuffer);
	}

	static void CreateTopLevelAccelerationStructure(HML::Mat4x4<float> bunnyTransformationMatrix = HML::Mat4x4<float>(1.0f), HML::Mat4x4<float> planeTransformationMatrix = HML::Mat4x4<float>(1.0f), bool Recreate = false)
	{
		vk::TransformMatrixKHR transformMatrix{};
		transformMatrix.matrix[0][0] = bunnyTransformationMatrix[0][0];
		transformMatrix.matrix[0][1] = bunnyTransformationMatrix[1][0];
		transformMatrix.matrix[0][2] = bunnyTransformationMatrix[2][0];
		transformMatrix.matrix[0][3] = bunnyTransformationMatrix[3][0];

		transformMatrix.matrix[1][0] = bunnyTransformationMatrix[0][1];
		transformMatrix.matrix[1][1] = bunnyTransformationMatrix[1][1];
		transformMatrix.matrix[1][2] = bunnyTransformationMatrix[2][1];
		transformMatrix.matrix[1][3] = bunnyTransformationMatrix[3][1];

		transformMatrix.matrix[2][0] = bunnyTransformationMatrix[0][2];
		transformMatrix.matrix[2][1] = bunnyTransformationMatrix[1][2];
		transformMatrix.matrix[2][2] = bunnyTransformationMatrix[2][2];
		transformMatrix.matrix[2][3] = bunnyTransformationMatrix[3][2];

		std::vector< vk::AccelerationStructureInstanceKHR> instances;
		vk::AccelerationStructureInstanceKHR instance{};
		instance.transform = transformMatrix;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = s_Data->bunny->m_meshes[0].m_bottomLevelAS.deviceAddress;

		instances.push_back(instance);

		transformMatrix.matrix[0][0] = planeTransformationMatrix[0][0];
		transformMatrix.matrix[0][1] = planeTransformationMatrix[1][0];
		transformMatrix.matrix[0][2] = planeTransformationMatrix[2][0];
		transformMatrix.matrix[0][3] = planeTransformationMatrix[3][0];

		transformMatrix.matrix[1][0] = planeTransformationMatrix[0][1];
		transformMatrix.matrix[1][1] = planeTransformationMatrix[1][1];
		transformMatrix.matrix[1][2] = planeTransformationMatrix[2][1];
		transformMatrix.matrix[1][3] = planeTransformationMatrix[3][1];

		transformMatrix.matrix[2][0] = planeTransformationMatrix[0][2];
		transformMatrix.matrix[2][1] = planeTransformationMatrix[1][2];
		transformMatrix.matrix[2][2] = planeTransformationMatrix[2][2];
		transformMatrix.matrix[2][3] = planeTransformationMatrix[3][2];

		instance.transform = transformMatrix;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = s_Data->plane->m_meshes[0].m_bottomLevelAS.deviceAddress;

		instances.push_back(instance);

		vk::BufferCreateInfo ibufferinfo{};
		ibufferinfo.flags = vk::BufferCreateFlags();
		ibufferinfo.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
		ibufferinfo.size = 4096 * sizeof(vk::AccelerationStructureInstanceKHR);
		ibufferinfo.sharingMode = vk::SharingMode::eExclusive;
		vk::Buffer instancesBuffer = Core::Context::_device->createBuffer(ibufferinfo);
		
		vk::DeviceMemory instancesBufferMemory;

		Core::Context::allocateBufferMemory(instancesBuffer, instancesBufferMemory, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::MemoryAllocateFlagBits::eDeviceAddress);

		//if (Recreate)
		//{
			void* imemlocation = Core::Context::_device->mapMemory(instancesBufferMemory, 0, sizeof(VkAccelerationStructureInstanceKHR));
			memcpy(imemlocation, instances.data(), 2 * sizeof(vk::AccelerationStructureInstanceKHR));
			Core::Context::_device->unmapMemory(instancesBufferMemory);
		//}

		vk::BufferDeviceAddressInfo info1;
		info1.buffer = instancesBuffer;
		vk::DeviceOrHostAddressConstKHR instanceDataDeviceAddress;
		instanceDataDeviceAddress.deviceAddress = Core::Context::_device->getBufferAddressKHR(info1);

		vk::AccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
		accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
		accelerationStructureGeometry.geometry.instances.sType = vk::StructureType::eAccelerationStructureGeometryInstancesDataKHR;
		accelerationStructureGeometry.geometry.instances.arrayOfPointers = false;
		accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

		// Get size info
		vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		accelerationStructureBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationStructureBuildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eUpdate;

		uint32_t primitive_count = 10;

		vk::AccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		Core::Context::_device->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &accelerationStructureBuildGeometryInfo, &primitive_count, &accelerationStructureBuildSizesInfo);

		vk::BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		bufferCreateInfo.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
		s_Data->TopLevelAccelerationStructure.buffer = Core::Context::_device->createBuffer(bufferCreateInfo);

		vk::MemoryRequirements memoryRequirements = Core::Context::_device->getBufferMemoryRequirements(s_Data->TopLevelAccelerationStructure.buffer);
		vk::MemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
		memoryAllocateFlagsInfo.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
		vk::MemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = Core::Context::findMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		Core::Context::_device->allocateMemory(&memoryAllocateInfo, nullptr, &s_Data->TopLevelAccelerationStructure.memory);
		Core::Context::_device->bindBufferMemory(s_Data->TopLevelAccelerationStructure.buffer, s_Data->TopLevelAccelerationStructure.memory, 0);
		// Acceleration structure
		vk::AccelerationStructureCreateInfoKHR accelerationStructureCreate_info{};
		accelerationStructureCreate_info.buffer = s_Data->TopLevelAccelerationStructure.buffer;
		accelerationStructureCreate_info.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreate_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;

		s_Data->TopLevelAccelerationStructure.handle = Core::Context::_device->createAccelerationStructureKHR(accelerationStructureCreate_info);

		// AS device address
		vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.accelerationStructure = s_Data->TopLevelAccelerationStructure.handle;
		s_Data->TopLevelAccelerationStructure.deviceAddress = Core::Context::_device->getAccelerationStructureAddressKHR(accelerationDeviceAddressInfo);

		// Create a small scratch buffer used during build of the top level acceleration structure
		Core::ScratchBuffer scratchBuffer = Core::createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		vk::AccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		accelerationBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
		accelerationBuildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
		accelerationBuildGeometryInfo.dstAccelerationStructure = s_Data->TopLevelAccelerationStructure.handle;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = 1;
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
		Core::Context::_device->destroyBuffer(instancesBuffer);
		Core::Context::_device->freeMemory(instancesBufferMemory);
		//if (Upadete)
		//{
		//	vk::WriteDescriptorSetAccelerationStructureKHR writev2;
		//	writev2.accelerationStructureCount = 1;
		//	writev2.pAccelerationStructures = &s_Data->TopLevelAccelerationStructure.handle;

		//	vk::WriteDescriptorSet write;
		//	write.sType = vk::StructureType::eWriteDescriptorSet;
		//	write.dstSet = s_Data->CamDescriptorSet;
		//	write.dstBinding = 1;
		//	write.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
		//	//write.pBufferInfo = &bufferinfov2; 
		//	write.descriptorCount = 1;
		//	write.pNext = &writev2;
		//
		//	Core::Context::_device->updateDescriptorSets(write, nullptr);
		//}
	}

	static void UpdateTopLevelAccelerationStructure(HML::Mat4x4<float> bunnyTransformationMatrix, HML::Mat4x4<float> planeTransformationMatrix)
	{
		vk::TransformMatrixKHR transformMatrix{};
		transformMatrix.matrix[0][0] = bunnyTransformationMatrix[0][0];
		transformMatrix.matrix[0][1] = bunnyTransformationMatrix[1][0];
		transformMatrix.matrix[0][2] = bunnyTransformationMatrix[2][0];
		transformMatrix.matrix[0][3] = bunnyTransformationMatrix[3][0];

		transformMatrix.matrix[1][0] = bunnyTransformationMatrix[0][1];
		transformMatrix.matrix[1][1] = bunnyTransformationMatrix[1][1];
		transformMatrix.matrix[1][2] = bunnyTransformationMatrix[2][1];
		transformMatrix.matrix[1][3] = bunnyTransformationMatrix[3][1];

		transformMatrix.matrix[2][0] = bunnyTransformationMatrix[0][2];
		transformMatrix.matrix[2][1] = bunnyTransformationMatrix[1][2];
		transformMatrix.matrix[2][2] = bunnyTransformationMatrix[2][2];
		transformMatrix.matrix[2][3] = bunnyTransformationMatrix[3][2];

		std::vector< vk::AccelerationStructureInstanceKHR> instances;
		vk::AccelerationStructureInstanceKHR instance{};
		instance.transform = transformMatrix;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = s_Data->bunny->m_meshes[0].m_bottomLevelAS.deviceAddress;

		instances.push_back(instance);

		transformMatrix.matrix[0][0] = planeTransformationMatrix[0][0];
		transformMatrix.matrix[0][1] = planeTransformationMatrix[1][0];
		transformMatrix.matrix[0][2] = planeTransformationMatrix[2][0];
		transformMatrix.matrix[0][3] = planeTransformationMatrix[3][0];

		transformMatrix.matrix[1][0] = planeTransformationMatrix[0][1];
		transformMatrix.matrix[1][1] = planeTransformationMatrix[1][1];
		transformMatrix.matrix[1][2] = planeTransformationMatrix[2][1];
		transformMatrix.matrix[1][3] = planeTransformationMatrix[3][1];

		transformMatrix.matrix[2][0] = planeTransformationMatrix[0][2];
		transformMatrix.matrix[2][1] = planeTransformationMatrix[1][2];
		transformMatrix.matrix[2][2] = planeTransformationMatrix[2][2];
		transformMatrix.matrix[2][3] = planeTransformationMatrix[3][2];

		instance.transform = transformMatrix;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = s_Data->plane->m_meshes[0].m_bottomLevelAS.deviceAddress;

		instances.push_back(instance);

		vk::BufferCreateInfo ibufferinfo{};
		ibufferinfo.flags = vk::BufferCreateFlags();
		ibufferinfo.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
		ibufferinfo.size = 4096 * sizeof(vk::AccelerationStructureInstanceKHR);
		ibufferinfo.sharingMode = vk::SharingMode::eExclusive;
		vk::Buffer instancesBuffer = Core::Context::_device->createBuffer(ibufferinfo);

		vk::DeviceMemory instancesBufferMemory;

		Core::Context::allocateBufferMemory(instancesBuffer, instancesBufferMemory, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::MemoryAllocateFlagBits::eDeviceAddress);

		void* imemlocation = Core::Context::_device->mapMemory(instancesBufferMemory, 0, sizeof(VkAccelerationStructureInstanceKHR));
		memcpy(imemlocation, instances.data(), instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
		Core::Context::_device->unmapMemory(instancesBufferMemory);

		vk::BufferDeviceAddressInfo info1;
		info1.buffer = instancesBuffer;

		vk::CommandBufferAllocateInfo CMDallocateInfo{};
		CMDallocateInfo.level = vk::CommandBufferLevel::ePrimary;
		CMDallocateInfo.commandPool = Core::Context::_graphicsCommandPool;
		CMDallocateInfo.commandBufferCount = 1;
		vk::CommandBuffer commandBufferUpadate = Core::Context::_device->allocateCommandBuffers(CMDallocateInfo)[0];
		vk::CommandBufferBeginInfo beg{};
		beg.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
		commandBufferUpadate.begin(beg);
		vk::MemoryBarrier barrier;
		barrier.sType = vk::StructureType::eMemoryBarrier;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR;
		commandBufferUpadate.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::DependencyFlags(), barrier, nullptr, nullptr);

		vk::DeviceOrHostAddressConstKHR instanceDataDeviceAddress;
		instanceDataDeviceAddress.deviceAddress = Core::Context::_device->getBufferAddressKHR(info1);

		vk::AccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
		accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
		accelerationStructureGeometry.geometry.instances.sType = vk::StructureType::eAccelerationStructureGeometryInstancesDataKHR;
		accelerationStructureGeometry.geometry.instances.arrayOfPointers = false;
		accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

		// Get size info
		vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		accelerationStructureBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationStructureBuildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eUpdate;
		accelerationStructureBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;

		uint32_t primitive_count = 1;

		vk::AccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		Core::Context::_device->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &accelerationStructureBuildGeometryInfo, &primitive_count, &accelerationStructureBuildSizesInfo);
		// Acceleration structure

		// AS device address

		// Create a small scratch buffer used during build of the top level acceleration structure
		Core::ScratchBuffer scratchBuffer = Core::createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

		accelerationStructureBuildGeometryInfo.srcAccelerationStructure = s_Data->TopLevelAccelerationStructure.handle;
		accelerationStructureBuildGeometryInfo.dstAccelerationStructure = s_Data->TopLevelAccelerationStructure.handle;
		accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = 1;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

		commandBufferUpadate.buildAccelerationStructuresKHR(accelerationStructureBuildGeometryInfo, accelerationBuildStructureRangeInfos);

		commandBufferUpadate.end();
		vk::SubmitInfo subInfo;
		subInfo.sType = vk::StructureType::eSubmitInfo;
		subInfo.commandBufferCount = 1;
		subInfo.pCommandBuffers = &commandBufferUpadate;
		Core::Context::_graphicsQueue.submit(subInfo);
		Core::Context::_device->waitIdle();
		Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, commandBufferUpadate);

		Core::deleteScratchBuffer(scratchBuffer);
		Core::Context::_device->destroyBuffer(instancesBuffer);
		Core::Context::_device->freeMemory(instancesBufferMemory);
	}

	static void CreateDescriptorSets()
	{
		vk::DescriptorSetLayoutBinding TopLevelAccelerationStructureBinding;
		TopLevelAccelerationStructureBinding.binding = 0;
		TopLevelAccelerationStructureBinding.descriptorCount = 1;
		TopLevelAccelerationStructureBinding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
		TopLevelAccelerationStructureBinding.pImmutableSamplers = nullptr;
		TopLevelAccelerationStructureBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR;

		vk::DescriptorSetLayoutCreateInfo TopLevelAccelerationStructureLayoutCreateInfo;
		TopLevelAccelerationStructureLayoutCreateInfo.bindingCount = 1;
		TopLevelAccelerationStructureLayoutCreateInfo.pBindings = &TopLevelAccelerationStructureBinding;

		s_Data->RT_SetLayout = Core::Context::_device->createDescriptorSetLayout(TopLevelAccelerationStructureLayoutCreateInfo);

		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = Core::Context::_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &s_Data->RT_SetLayout;

		s_Data->Rt_Set = Core::Context::_device->allocateDescriptorSets(allocInfo)[0];

		vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
		descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
		descriptorAccelerationStructureInfo.pAccelerationStructures = &s_Data->TopLevelAccelerationStructure.handle;

		vk::DescriptorImageInfo imginfo;


		std::array<vk::WriteDescriptorSet, 2> write;
		write[0].sType = vk::StructureType::eWriteDescriptorSet;
		write[0].pNext = &descriptorAccelerationStructureInfo;
		write[0].dstSet = s_Data->Rt_Set;
		write[0].dstBinding = 0;
		write[0].descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
		write[0].descriptorCount = 1;

		write[1].sType = vk::StructureType::eWriteDescriptorSet;
		write[1].pImageInfo = &imginfo;
		write[1].dstSet = s_Data->Rt_Set;
		write[1].dstBinding = 1;
		write[1].descriptorType = vk::DescriptorType::eStorageImage;
		write[1].descriptorCount = 1;

		Core::Context::_device->updateDescriptorSets(write, nullptr);
	}

	static void CreateCameraUniformBuffer()
	{

	}

	static void Create()
	{
		Data::DepthAttachment dAttachment;
		for (int i = 0; i < Core::Context::_swapchain.swapchainImageViews.size(); ++i)
		{
			vk::ImageCreateInfo imageInfo;
			imageInfo.imageType = vk::ImageType::e2D;
			imageInfo.extent.width = Core::Context::_swapchain.swapchainWidth;
			imageInfo.extent.height = Core::Context::_swapchain.swapchainHeight;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = vk::Format::eD32SfloatS8Uint;
			imageInfo.tiling = vk::ImageTiling::eOptimal;
			imageInfo.initialLayout = vk::ImageLayout::eUndefined;
			imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
			imageInfo.sharingMode = vk::SharingMode::eExclusive;
			imageInfo.samples = vk::SampleCountFlagBits::e1;

			dAttachment.image = Core::Context::_device->createImage(imageInfo);
			dAttachment.MemRequs = Core::Context::_device->getImageMemoryRequirements(dAttachment.image);
			vk::MemoryAllocateInfo memAlloc;
			memAlloc.sType = vk::StructureType::eMemoryAllocateInfo;
			memAlloc.allocationSize = dAttachment.MemRequs.size;
			memAlloc.memoryTypeIndex = Core::Context::findMemoryType(dAttachment.MemRequs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);		//FIX
			dAttachment.Memory = Core::Context::_device->allocateMemory(memAlloc);
			Core::Context::_device->bindImageMemory(dAttachment.image, dAttachment.Memory, 0);

			vk::ImageViewCreateInfo viewInfo;
			viewInfo.viewType = vk::ImageViewType::e2D;
			viewInfo.format = vk::Format::eD32SfloatS8Uint;
			viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			viewInfo.image = dAttachment.image;
			dAttachment.view = Core::Context::_device->createImageView(viewInfo);
			
			s_Data->depth_attachment.push_back(dAttachment);
		}
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

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		inputAssemblyInfo.flags = vk::PipelineInputAssemblyStateCreateFlags();
		inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;

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
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		vk::PipelineDepthStencilStateCreateInfo depthStencilInfo;
		depthStencilInfo.sType = vk::StructureType::ePipelineDepthStencilStateCreateInfo;
		depthStencilInfo.depthTestEnable = true;
		depthStencilInfo.depthWriteEnable = true;
		depthStencilInfo.depthCompareOp = vk::CompareOp::eLess;
		depthStencilInfo.depthBoundsTestEnable = false;
		depthStencilInfo.stencilTestEnable = false;

		//Pipeline Layout
#ifdef M_DEBUG
		std::cout << "Create Pipeline Layout" << std::endl;
#endif
		std::array<vk::DescriptorSetLayout, 1> layouts = { s_Data->CamDescriptorSetLayout };

		vk::PipelineLayoutCreateInfo layoutInfo;
		layoutInfo.flags = vk::PipelineLayoutCreateFlags();
		layoutInfo.setLayoutCount = layouts.size();
		layoutInfo.pSetLayouts = layouts.data();
		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &range;

		s_Data->pipelineLayout = Core::Context::_device->createPipelineLayout(layoutInfo);


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

		vk::AttachmentDescription depthAttachment{};
		depthAttachment.flags = vk::AttachmentDescriptionFlags();
		depthAttachment.format = vk::Format::eD32SfloatS8Uint;
		depthAttachment.samples = vk::SampleCountFlagBits::e1;
		depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
		depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		//Declare that attachment to be color buffer 0 of the framebuffer
		vk::AttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::AttachmentReference depthStencilAttachmentRef = {};
		depthStencilAttachmentRef.attachment = 1;
		depthStencilAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		//Renderpasses are broken down into subpasses, there's always at least one.
		vk::SubpassDescription subpass{};
		subpass.flags = vk::SubpassDescriptionFlags();
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthStencilAttachmentRef;

		std::array<vk::AttachmentDescription, 2> attachments{ colorAttachment, depthAttachment };

		//Now create the renderpass
		vk::RenderPassCreateInfo renderpassInfo{};
		renderpassInfo.flags = vk::RenderPassCreateFlags();
		renderpassInfo.attachmentCount = attachments.size();
		renderpassInfo.pAttachments = attachments.data();
		renderpassInfo.subpassCount = 1;
		renderpassInfo.pSubpasses = &subpass;
		s_Data->renderpass = Core::Context::_device->createRenderPass(renderpassInfo);


		//Extra stuff

		//Make the Pipeline
#ifdef M_DEBUG
		std::cout << "Create Graphics Pipeline" << std::endl;
#endif
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
		pipelineInfo.layout = s_Data->pipelineLayout;
		pipelineInfo.renderPass = s_Data->renderpass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = nullptr;

		s_Data->graphicsPipeline = Core::Context::_device->createGraphicsPipeline(nullptr, pipelineInfo).value;

		Core::Context::_device->destroyShaderModule(vertexShader);
		Core::Context::_device->destroyShaderModule(fragmentShader);

		for (int i = 0; i < Core::Context::_swapchain.swapchainImageViews.size(); ++i) {

			std::vector<vk::ImageView> attchs = { Core::Context::_swapchain.swapchainImageViews[i], s_Data->depth_attachment[i].view };
			vk::FramebufferCreateInfo framebufferInfo;
			framebufferInfo.flags = vk::FramebufferCreateFlags();
			framebufferInfo.renderPass = s_Data->renderpass;
			framebufferInfo.attachmentCount = attchs.size();
			framebufferInfo.pAttachments = attchs.data();
			framebufferInfo.width = Core::Context::_swapchain.extent.width;
			framebufferInfo.height = Core::Context::_swapchain.extent.height;
			framebufferInfo.layers = 1;
			s_Data->framebuffers.push_back(Core::Context::_device->createFramebuffer(framebufferInfo));
		}
		vk::CommandBufferAllocateInfo allocInfo{};
		allocInfo.commandPool = Core::Context::_graphicsCommandPool;
		allocInfo.level = vk::CommandBufferLevel::ePrimary;
		allocInfo.commandBufferCount = s_Data->framebuffers.size();
		s_Data->cmd = Core::Context::_device->allocateCommandBuffers(allocInfo);
	}

	RayTracing::RayTracing()
	{
		s_Data->pushConstant.ModelTransform = HML::Mat4x4<>(1.0f);
		s_Data->bunny = std::make_shared<Model>("res/bunny.obj");
		s_Data->plane = std::make_shared<Model>("res/Plane.gltf");

		CreateTopLevelAccelerationStructure(HML::Mat4x4<float>(1.0f), HML::Mat4x4<float>(1.0f));
		CreateStorageImage(vk::Format::eB8G8R8A8Unorm);

		s_Data->cam = std::make_shared<Camera>(HML::Radians(45.0f), float(Core::Context::_swapchain.extent.width) 
			/ float(Core::Context::_swapchain.extent.height), 0.1f, 1000.0f);

		vk::DescriptorSetLayoutBinding binding;
		binding.binding = 0;
		binding.descriptorType = vk::DescriptorType::eUniformBuffer;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

		std::vector<vk::DescriptorSetLayoutBinding> bindings;

		bindings.push_back(binding);

		binding.binding = 1;
		binding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		bindings.push_back(binding);

		vk::DescriptorSetLayoutCreateInfo layoutinfo;
		layoutinfo.bindingCount = bindings.size();
		layoutinfo.pBindings = bindings.data();

		s_Data->CamDescriptorSetLayout = Core::Context::_device->createDescriptorSetLayout(layoutinfo);

		vk::DescriptorSetAllocateInfo info;
		info.sType = vk::StructureType::eDescriptorSetAllocateInfo;
		info.descriptorPool = Core::Context::_descriptorPool;
		info.descriptorSetCount = 1;
		info.pSetLayouts = &s_Data->CamDescriptorSetLayout;

		s_Data->CamDescriptorSet = Core::Context::_device->allocateDescriptorSets(info).front();

		vk::DescriptorBufferInfo bufferinfo;
		bufferinfo.buffer = s_Data->cam->m_CameraUniformBuffer.buffer;
		bufferinfo.offset = 0;
		bufferinfo.range = sizeof(CameraData);
		vk::WriteDescriptorSet write;
		write.sType = vk::StructureType::eWriteDescriptorSet;
		write.dstSet = s_Data->CamDescriptorSet;
		write.dstBinding = 0;
		write.descriptorType = vk::DescriptorType::eUniformBuffer;
		write.pBufferInfo = &bufferinfo;
		write.descriptorCount = 1;

		Core::Context::_device->updateDescriptorSets(write, nullptr);

		vk::WriteDescriptorSetAccelerationStructureKHR writev2;
		writev2.accelerationStructureCount = 1;
		writev2.pAccelerationStructures = &s_Data->TopLevelAccelerationStructure.handle;
		
		write.sType = vk::StructureType::eWriteDescriptorSet;
		write.dstSet = s_Data->CamDescriptorSet;
		write.dstBinding = 1;
		write.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
		//write.pBufferInfo = &bufferinfov2; 
		write.descriptorCount = 1;
		write.pNext = &writev2;

		Core::Context::_device->updateDescriptorSets(write, nullptr);
		//CreateDescriptorSets();


		s_Data->bdesc.binding = 0;
		s_Data->bdesc.inputRate = vk::VertexInputRate::eVertex;
		s_Data->bdesc.stride = sizeof(Vertex);

		vk::VertexInputAttributeDescription attr01;
		attr01.binding = 0;
		attr01.location = 0;
		attr01.format = vk::Format::eR32G32B32Sfloat;
		attr01.offset = offsetof(Vertex, Vertex::position);;
		s_Data->attributes.push_back(attr01);

		vk::VertexInputAttributeDescription attr02;
		attr02.binding = 0;
		attr02.location = 1;
		attr02.format = vk::Format::eR32G32B32Sfloat;
		attr02.offset = offsetof(Vertex, Vertex::normal);
		s_Data->attributes.push_back(attr02);

		vk::VertexInputAttributeDescription attr03;
		attr01.binding = 0;
		attr01.location = 2;
		attr01.format = vk::Format::eR32G32B32A32Sfloat;
		attr01.offset = offsetof(Vertex, Vertex::tangent);
		s_Data->attributes.push_back(attr01);

		vk::VertexInputAttributeDescription attr04;
		attr02.binding = 0;
		attr02.location = 3;
		attr02.format = vk::Format::eR32G32B32A32Sfloat;
		attr02.offset = offsetof(Vertex, Vertex::bitangent);
		s_Data->attributes.push_back(attr02);

		vk::VertexInputAttributeDescription attr05;
		attr02.binding = 0;
		attr02.location = 4;
		attr02.format = vk::Format::eR32G32B32A32Sfloat;
		attr02.offset = offsetof(Vertex, Vertex::color);
		s_Data->attributes.push_back(attr02);

		vk::VertexInputAttributeDescription attr06;
		attr02.binding = 0;
		attr02.location = 5;
		attr02.format = vk::Format::eR32G32Sfloat;
		attr02.offset = offsetof(Vertex, Vertex::uv);
		s_Data->attributes.push_back(attr02);

		Create();
	}

	void RayTracing::OnUpdate()
	{
		HML::Mat4x4<float> temp = HML::Mat4x4<float>(1.0f);

		//Simple scale matrix for plane
		temp[0] = temp[0] * 10.0f;
		temp[1] = temp[1] * 10.0f;
		temp[2] = temp[2] * 10.0f;
		temp[3] = temp[3];
		temp[3][1] = -1.0f;

		UpdateTopLevelAccelerationStructure(s_Data->pushConstant.ModelTransform, temp);

		s_Data->cam->OnUpdate();

		vk::CommandBufferBeginInfo beginInfo{};

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].begin(beginInfo);

		vk::RenderPassBeginInfo renderPassInfo{};
		renderPassInfo.renderPass = s_Data->renderpass;
		renderPassInfo.framebuffer = s_Data->framebuffers[Core::Context::_swapchain.currentFrame];
		renderPassInfo.renderArea.offset.x = 0;
		renderPassInfo.renderArea.offset.y = 0;
		renderPassInfo.renderArea.extent = Core::Context::_swapchain.extent;

		vk::ClearValue clearColor{ std::array<float, 4>{0.2f, 0.5f, 0.7f, 1.0f} };
		vk::ClearValue clearDepth;
		clearDepth.depthStencil.depth = 1.0f;
		std::vector<vk::ClearValue> values { clearColor, clearDepth };
		renderPassInfo.clearValueCount = values.size();
		renderPassInfo.pClearValues = values.data();

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindPipeline(vk::PipelineBindPoint::eGraphics, s_Data->graphicsPipeline);

		std::array<vk::DescriptorSet, 1> sets = { s_Data->CamDescriptorSet };

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, s_Data->pipelineLayout, 0, sets, {  });

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].pushConstants(s_Data->pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(Data::Constant), &s_Data->pushConstant);
		
		s_Data->bunny->DrawModel(s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage]);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].pushConstants(s_Data->pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(Data::Constant), &temp);
		
		s_Data->plane->DrawModel(s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage]);

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].endRenderPass();

		s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage].end();

		vk::SubmitInfo submit_info{};
		submit_info.sType = vk::StructureType::eSubmitInfo;
		vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		submit_info.pWaitDstStageMask = &waitStage;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &Core::Context::imageAvailableSemaphores[Core::Context::_swapchain.currentFrame];
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &Core::Context::renderFinishedSemaphores[Core::Context::_swapchain.currentFrame];;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &s_Data->cmd[Core::Context::_swapchain.nextSwapchainImage];

		Core::Context::_device->resetFences(Core::Context::inFlightFences[Core::Context::_swapchain.currentFrame]);

		Core::Context::_graphicsQueue.submit(submit_info, Core::Context::inFlightFences[Core::Context::_swapchain.currentFrame]);
	}

	void RayTracing::Shutdown()
	{
		s_Data->cam->Destroy();
		s_Data->bunny->Destroy();
		s_Data->plane->Destroy();
		Core::Context::_device->destroyDescriptorSetLayout(s_Data->CamDescriptorSetLayout);
		Core::Context::_device->freeDescriptorSets(Core::Context::_descriptorPool, s_Data->CamDescriptorSet);
		Destroy();
		delete s_Data;
	}

	void RayTracing::Destroy()
	{
		Core::Context::_device->destroyPipelineLayout(s_Data->pipelineLayout);
		Core::Context::_device->destroyPipeline(s_Data->graphicsPipeline);
		for (auto framebuffer : s_Data->framebuffers)
			Core::Context::_device->destroyFramebuffer(framebuffer);
		s_Data->framebuffers.clear();
		for (auto attachment : s_Data->depth_attachment)
		{
			Core::Context::_device->destroyImageView(attachment.view);
			Core::Context::_device->freeMemory(attachment.Memory);
			Core::Context::_device->destroyImage(attachment.image);
		}
		s_Data->depth_attachment.clear();
		Core::Context::_device->freeCommandBuffers(Core::Context::_graphicsCommandPool, s_Data->cmd);
		s_Data->cmd.clear();
		Core::Context::_device->destroyRenderPass(s_Data->renderpass);
	}

	void RayTracing::Recreate()
	{
		Create();
	}
}