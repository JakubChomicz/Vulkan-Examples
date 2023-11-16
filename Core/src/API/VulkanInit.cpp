#include "pch.hpp"
#include "VulkanInit.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Core
{
	std::unique_ptr<Context> Context::s_Context;
#ifdef M_DEBUG
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessegerCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}
#endif
	uint32_t Context::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
		auto memProperties = m_GPU.getMemoryProperties();

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
#ifdef M_DEBUG
		std::cout << "Unknown Memory Type" << std::endl;
#endif
		return -1;
	}
	void Context::allocateBufferMemory(vk::Buffer& buffer, vk::DeviceMemory& mem, vk::MemoryPropertyFlags flags, vk::MemoryAllocateFlags allocateFlags)
	{
		vk::MemoryRequirements memoryRequirements = Core::Context::m_Device->getBufferMemoryRequirements(buffer);
		vk::MemoryAllocateInfo allocInfo;
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = Core::Context::findMemoryType(memoryRequirements.memoryTypeBits,
			flags
		);
		vk::MemoryAllocateFlagsInfo flagsInfo;
		if (allocateFlags != vk::MemoryAllocateFlagBits::eDeviceAddressCaptureReplay)
		{
			flagsInfo.flags = allocateFlags;
			allocInfo.pNext = &flagsInfo;
		}

		mem = Core::Context::m_Device->allocateMemory(allocInfo);
		Core::Context::m_Device->bindBufferMemory(buffer, mem, 0);
	}
	Context::Context()
	{
		vk::DynamicLoader dl;
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

		uint32_t glfwExtensionsCount = 0;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionsCount);
		m_InstanceExtensions = extensions;
		//Instance
		{
#ifdef M_DEBUG
			_layers.push_back("VK_LAYER_KHRONOS_validation");
			_instanceExtensions.push_back("VK_EXT_debug_utils");
#endif
			vk::ApplicationInfo appInfo;
			appInfo.sType = vk::StructureType::eApplicationInfo;
			appInfo.pApplicationName = "Unused";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 3);
			appInfo.pEngineName = "Azazel Engine";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 3);
			appInfo.apiVersion = VK_API_VERSION_1_3;

			vk::InstanceCreateInfo instanceInfo;
			instanceInfo.sType = vk::StructureType::eInstanceCreateInfo;
			instanceInfo.flags = vk::InstanceCreateFlags();
			instanceInfo.enabledLayerCount = static_cast<uint32_t>(m_Layers.size());
			instanceInfo.ppEnabledLayerNames = m_Layers.data();
			instanceInfo.enabledExtensionCount = static_cast<uint32_t>(m_InstanceExtensions.size());
			instanceInfo.ppEnabledExtensionNames = m_InstanceExtensions.data();
			instanceInfo.pApplicationInfo = &appInfo;

			m_Instance = vk::createInstanceUnique(instanceInfo);
			VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_Instance);
		}
		//Debug Messenger
		{
#ifdef M_DEBUG
			_dispatcher = vk::DispatchLoaderDynamic(*_instance, vkGetInstanceProcAddr);
			vk::DebugUtilsMessengerCreateInfoEXT debugInfo;
			debugInfo.messageSeverity =
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
			debugInfo.messageType =
				vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
				vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
				vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
			debugInfo.pfnUserCallback = debugMessegerCallback;
			_debugMessenger = _instance->createDebugUtilsMessengerEXTUnique(debugInfo, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER);
#endif
		}
	}
	Context::~Context()
	{
		Context::DestroySwapchain();
		m_Device->destroyCommandPool(m_GraphicsCommandPool);
		m_Device->destroyCommandPool(m_TransferCommandPool);
		m_Device->destroyCommandPool(m_ComputeCommandPool);
		m_Device->destroyDescriptorPool(m_DescriptorPool);
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			Context::m_Device->destroySemaphore(m_ImageAvailableSemaphores[i]);
			Context::m_Device->destroySemaphore(m_RenderFinishedSemaphores[i]);
			Context::m_Device->destroyFence(m_InFlightFences[i]);
		}
	}
	void Context::Init()
	{
		s_Context = std::make_unique<Context>();
	}
	void Context::Shutdown()
	{
		s_Context.reset();
		s_Context = nullptr;
	}
	void Context::SetWindowSurface(void* window)
	{
		VkSurfaceKHR raw;
		glfwCreateWindowSurface(m_Instance.get(), (GLFWwindow*)window, nullptr, &raw);
		vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE> _deleter(m_Instance.get());
		m_Surface = vk::UniqueSurfaceKHR(vk::SurfaceKHR(raw), _deleter);
	}
	void Context::InitDevice()
	{
		//GPU
		{
			std::vector<vk::PhysicalDevice> _avaibleGPUs = m_Instance->enumeratePhysicalDevices();
			std::cout << ("Enumerating GPUs...") << std::endl;
			{
				std::multimap<uint32_t, vk::PhysicalDevice> GPUsChooseable;
				for (auto gpu : _avaibleGPUs)
				{
					uint32_t score = 0;
					if (gpu.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
					{
						score += 1000;
					}
					score += gpu.getProperties().limits.maxColorAttachments;
					score += gpu.getProperties().limits.maxCullDistances;
					score += gpu.getProperties().limits.maxImageDimension2D;
					score += gpu.getProperties().limits.maxDescriptorSetStorageImages;
					score += gpu.getProperties().limits.maxDescriptorSetStorageBuffers;
					score += gpu.getProperties().limits.maxDescriptorSetStorageBuffersDynamic;
					score += gpu.getProperties().limits.maxDescriptorSetUniformBuffers;
					score += gpu.getProperties().limits.maxDescriptorSetUniformBuffersDynamic;
					score += gpu.getProperties().limits.maxComputeSharedMemorySize;
					score += gpu.getProperties().limits.maxComputeWorkGroupInvocations;
					gpu.getFeatures().geometryShader ? score = score : score = 0;

					GPUsChooseable.insert(std::make_pair(score, gpu));
				}
				m_GPU = GPUsChooseable.rbegin()->second;
			}
			 std::cout << "Selected GPU: " << m_GPU.getProperties().deviceName << std::endl;
			 std::cout << "Vulkan API Version " <<
				VK_VERSION_MAJOR(m_GPU.getProperties().apiVersion)  << "." <<
				VK_VERSION_MINOR(m_GPU.getProperties().apiVersion) << "." <<
				VK_VERSION_PATCH(m_GPU.getProperties().apiVersion) << std::endl;
			std::cout << "Getting " << m_GPU.getProperties().deviceName << " QueuesFamiliesIndexes:" << std::endl;
			auto queueFamilyProps = m_GPU.getQueueFamilyProperties();
			uint8_t min_transfer_score = 255;
			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProps.size()); ++i)
			{
				uint8_t current_transfer_score = 0;

				if (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eGraphics)
				{
					m_QueueFamilyIndices.graphicsFamily = i;
					++current_transfer_score;
				}
				if (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eCompute)
				{
					m_QueueFamilyIndices.computeFamily = i;
					++current_transfer_score;
				}
				if (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eTransfer)
				{
					if (current_transfer_score <= min_transfer_score)
					{
						min_transfer_score = current_transfer_score;
						m_QueueFamilyIndices.transferFamily = i;
					}
				}
			}
			std::cout << "\tGraphics Queue Family Index: "  << m_QueueFamilyIndices.graphicsFamily	<< std::endl;
			std::cout << "\tCompute Queue  Family Index: "  << m_QueueFamilyIndices.computeFamily	<< std::endl;
			std::cout << "\tTransfer Queue  Family Index: " << m_QueueFamilyIndices.transferFamily	<< std::endl;

			std::set<uint32_t> uniqueQueueFamilies = { m_QueueFamilyIndices.graphicsFamily,
			m_QueueFamilyIndices.transferFamily,
			m_QueueFamilyIndices.computeFamily };
			m_QueueFamilyIndices.QueueFamilies = std::vector(uniqueQueueFamilies.begin(), uniqueQueueFamilies.end());
			m_QueueFamilyIndices.graphicsQueueIndex = m_QueueFamilyIndices.graphicsFamily;
			m_QueueFamilyIndices.transferQueueIndex = m_QueueFamilyIndices.transferFamily;
			m_QueueFamilyIndices.computeQueueIndex = m_QueueFamilyIndices.computeFamily;
		}
		//Device
		bool hasAccelerationStructureSupport = false;
		{
			vk::PhysicalDeviceFeatures2 features2{};
			m_DeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			auto extensions = m_GPU.enumerateDeviceExtensionProperties();
			for (auto ex : extensions)
			{
				std::string name = ex.extensionName.data();
				if (name == VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)
				{
					m_DeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
				}
				else if (name == VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
				{
					m_DeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
				}
				else if (name == VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME)
				{
					m_DeviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
				}
				else if (name == VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
				{
					m_DeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
					hasAccelerationStructureSupport = true;
				}
				else if (name == VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
				{
					m_DeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
				}
				else if (name == VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
				{
					m_DeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
				}
			}
			vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature {};
			rtPipelineFeature.rayTracingPipeline = true;
			vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelFeature {};
			accelFeature.accelerationStructure = true;
			vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeature {};
			bufferDeviceAddressFeature.bufferDeviceAddress = true;
			vk::PhysicalDeviceDescriptorIndexingFeatures descriptorIndexing {};

			features2.pNext = &descriptorIndexing;
			descriptorIndexing.pNext = &accelFeature;
			accelFeature.pNext = &rtPipelineFeature;
			rtPipelineFeature.pNext = &bufferDeviceAddressFeature;


			m_GPU.getFeatures2(&features2);

			vk::PhysicalDeviceProperties2 prop2 {};
			vk::PhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties {};
			prop2.pNext = &m_rtProperties;
			m_GPU.getProperties2(&prop2);

			uint32_t index_count = 1;;
			if (!m_QueueFamilyIndices.doesTransferShareGraphicsQueue())
				index_count++;
			if (!m_QueueFamilyIndices.doesComputeShareGraphicsQueue())
				index_count++;
			std::vector<uint32_t> indices(index_count);
			uint8_t index = 0;
			indices[index++] = m_QueueFamilyIndices.graphicsQueueIndex;
			if (!m_QueueFamilyIndices.doesTransferShareGraphicsQueue())
				indices[index++] = m_QueueFamilyIndices.transferQueueIndex;
			if (!m_QueueFamilyIndices.doesComputeShareGraphicsQueue())
				indices[index++] = m_QueueFamilyIndices.computeQueueIndex;

			std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos(index_count);
			for (uint32_t i = 0; i < index_count; ++i)
			{
				queueCreateInfos[i].sType = vk::StructureType::eDeviceQueueCreateInfo;
				queueCreateInfos[i].queueFamilyIndex = indices[i];
				queueCreateInfos[i].queueCount = 1;
				//TODO: Enable in feature for multithreaded rendering
				//if (indices[i] == m_queueFamilyIndices.m_graphicsQueueIndex)
				//	queueCreateInfos[i].queueCount = 2;
				queueCreateInfos[i].flags = (vk::DeviceQueueCreateFlags)0;
				queueCreateInfos[i].pNext = 0;
				float queuePriority = 1.0f;		//FIX maybe
				queueCreateInfos[i].pQueuePriorities = &queuePriority;
			}

			vk::DeviceCreateInfo deviceInfo;
			deviceInfo.sType = vk::StructureType::eDeviceCreateInfo;
			deviceInfo.queueCreateInfoCount = index_count;
			deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
			deviceInfo.enabledLayerCount = static_cast<uint32_t>(m_Layers.size());
			deviceInfo.ppEnabledLayerNames = m_Layers.data();
			deviceInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
			deviceInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();
			deviceInfo.pEnabledFeatures = nullptr;
			deviceInfo.pNext = &features2;

			m_Device = m_GPU.createDeviceUnique(deviceInfo);
			VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_Device);

			m_GraphicsQueue = m_Device->getQueue(m_QueueFamilyIndices.graphicsFamily, 0);
			m_TransferQueue = m_Device->getQueue(m_QueueFamilyIndices.transferFamily, 0);
			m_ComputeQueue = m_Device->getQueue(m_QueueFamilyIndices.computeFamily, 0);
		}
		//GraphicsCommandPool
		{
			vk::CommandPoolCreateInfo commandPoolInfo;
			commandPoolInfo.sType = vk::StructureType::eCommandPoolCreateInfo;
			commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;		//Might need Fix
			commandPoolInfo.queueFamilyIndex = m_QueueFamilyIndices.graphicsFamily;
			m_GraphicsCommandPool = m_Device->createCommandPool(commandPoolInfo);
		}
		//TransferCommandPool
		{
			vk::CommandPoolCreateInfo commandPoolInfo;
			commandPoolInfo.sType = vk::StructureType::eCommandPoolCreateInfo;
			commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;		//Might need Fix
			commandPoolInfo.queueFamilyIndex = m_QueueFamilyIndices.transferFamily;
			m_TransferCommandPool = m_Device->createCommandPool(commandPoolInfo);
		}
		//TransferCommandPool
		{
			vk::CommandPoolCreateInfo commandPoolInfo;
			commandPoolInfo.sType = vk::StructureType::eCommandPoolCreateInfo;
			commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;		//Might need Fix
			commandPoolInfo.queueFamilyIndex = m_QueueFamilyIndices.computeFamily;
			m_ComputeCommandPool = m_Device->createCommandPool(commandPoolInfo);
		}
		//Descriptor Pool
		{
			std::vector<vk::DescriptorPoolSize> pool_sizes =
			{
				{ vk::DescriptorType::eSampler, 1000 },
				{ vk::DescriptorType::eCombinedImageSampler, 1000 },
				{  vk::DescriptorType::eSampledImage, 1000 },
				{  vk::DescriptorType::eStorageImage, 1000 },
				{  vk::DescriptorType::eUniformTexelBuffer, 1000 },
				{  vk::DescriptorType::eStorageTexelBuffer, 1000 },
				{  vk::DescriptorType::eUniformBuffer, 1000 },
				{  vk::DescriptorType::eStorageBuffer, 1000 },
				{  vk::DescriptorType::eUniformBufferDynamic, 1000 },
				{  vk::DescriptorType::eStorageBufferDynamic, 1000 },
				{  vk::DescriptorType::eInputAttachment, 1000 }
			};
			if(hasAccelerationStructureSupport) 
				pool_sizes.push_back({ vk::DescriptorType::eAccelerationStructureKHR, 1000 });
			vk::DescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = vk::StructureType::eDescriptorPoolCreateInfo;
			pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
			pool_info.maxSets = 1000 * ((int)(sizeof(pool_sizes) / sizeof(pool_sizes.size() * sizeof(vk::DescriptorPoolSize))));
			pool_info.poolSizeCount = (uint32_t)((int)(sizeof(pool_sizes) / sizeof(pool_sizes.size() * sizeof(vk::DescriptorPoolSize))));
			pool_info.pPoolSizes = pool_sizes.data();
			m_DescriptorPool = Context::m_Device->createDescriptorPool(pool_info);
		}
		m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		vk::SemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = vk::StructureType::eSemaphoreCreateInfo;
		vk::FenceCreateInfo renderFenceInfo;
		renderFenceInfo.sType = vk::StructureType::eFenceCreateInfo;
		renderFenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_ImageAvailableSemaphores[i] = m_Device->createSemaphore(semaphoreInfo);
			m_RenderFinishedSemaphores[i] = m_Device->createSemaphore(semaphoreInfo);
			m_InFlightFences[i] = m_Device->createFence(renderFenceInfo);
		}
	}
	////////////////////////////////////////////////////////////////////////////////////Objects-Creation///////////////////////////////////////////////////////////////////
	void Context::CreateSwapchain(uint32_t width, uint32_t height)
	{
		m_Swapchain.swapchainWidth = width;
		m_Swapchain.swapchainHeight = height;
		m_Swapchain.extent =
			vk::Extent2D{ static_cast<uint32_t>(m_Swapchain.swapchainWidth), static_cast<uint32_t>(m_Swapchain.swapchainHeight) };

		for (auto mode : m_GPU.getSurfacePresentModesKHR(m_Surface.get()))
		{
			if (mode == vk::PresentModeKHR::eFifo || mode == vk::PresentModeKHR::eMailbox)
				m_Swapchain.presentMode = mode;
			if (mode == vk::PresentModeKHR::eFifo)
				break;
		}
		vk::SwapchainCreateInfoKHR swapchainInfo;
		swapchainInfo.sType = vk::StructureType::eSwapchainCreateInfoKHR;
		swapchainInfo.surface = *m_Surface;
		swapchainInfo.minImageCount = m_GPU.getSurfaceCapabilitiesKHR(*m_Surface).minImageCount/* + 1*/;
		swapchainInfo.imageFormat = vk::Format::eB8G8R8A8Unorm;
		swapchainInfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
		swapchainInfo.imageExtent = m_Swapchain.extent;
		swapchainInfo.imageArrayLayers = 1;
		swapchainInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
		swapchainInfo.imageSharingMode = vk::SharingMode::eExclusive;
		swapchainInfo.queueFamilyIndexCount = 0;
		swapchainInfo.pQueueFamilyIndices = nullptr;
		swapchainInfo.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
		swapchainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		swapchainInfo.presentMode = m_Swapchain.presentMode;
		swapchainInfo.clipped = true;
		swapchainInfo.oldSwapchain = m_Swapchain.oldswapchain;

		m_Swapchain.swapchain = m_Device->createSwapchainKHR(swapchainInfo);
	}
	void Context::CreateSwapchainImages()
	{
		m_Swapchain.swapchainImages = m_Device->getSwapchainImagesKHR(m_Swapchain.swapchain);
		m_Swapchain.swapchainImageViews.resize(m_Swapchain.swapchainImages.size());
		for (size_t i = 0; i < m_Swapchain.swapchainImages.size(); i++) {
			vk::ImageViewCreateInfo imageViewInfo;
			imageViewInfo.sType = vk::StructureType::eImageViewCreateInfo;
			imageViewInfo.image = m_Swapchain.swapchainImages[i];
			imageViewInfo.viewType = vk::ImageViewType::e2D;
			imageViewInfo.format = vk::Format::eB8G8R8A8Unorm;
			imageViewInfo.components = vk::ComponentMapping{ vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
			imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			imageViewInfo.subresourceRange.baseMipLevel = 0;
			imageViewInfo.subresourceRange.levelCount = 1;
			imageViewInfo.subresourceRange.baseArrayLayer = 0;
			imageViewInfo.subresourceRange.layerCount = 1;

			m_Swapchain.swapchainImageViews[i] = m_Device->createImageView(imageViewInfo);
		}
	}
	void Context::AcquireNextSwapchainImage()
	{
		m_Device->waitForFences(m_InFlightFences[m_Swapchain.currentFrame], true, UINT64_MAX);
		auto acquireResult = m_Device->acquireNextImageKHR(m_Swapchain.swapchain, UINT64_MAX, m_ImageAvailableSemaphores[m_Swapchain.currentFrame]);
		m_Swapchain.nextSwapchainImage = acquireResult.value;
	}
	void Context::DestroySwapchain()
	{
		for (auto& imageView : m_Swapchain.swapchainImageViews) {
			m_Device->destroyImageView(imageView);
		}
		m_Device->destroySwapchainKHR(m_Swapchain.swapchain);
	}
	void Context::RecreateSwapchain(uint32_t width, uint32_t height)
	{
		WaitIdle();
		CreateSwapchain(width, height);
		CreateSwapchainImages();
		WaitIdle();
	}
	void Context::Present()
	{
		vk::PresentInfoKHR presentInfo;
		presentInfo.sType = vk::StructureType::ePresentInfoKHR;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_Swapchain.swapchain;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[m_Swapchain.currentFrame];
		presentInfo.pImageIndices = &m_Swapchain.nextSwapchainImage;
		m_GraphicsQueue.presentKHR(presentInfo);
		m_Swapchain.currentFrame = (m_Swapchain.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}
	////////////////////////////////////////////////////////////////////////////////////////COMMANDS////////////////////////////////////////////////////////////////////
	void Context::WaitIdle()
	{
		m_Device->waitIdle();
	}
	static uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
		auto memProperties = Core::Context::Get()->GetGPU().getMemoryProperties();

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		return -1;
	}
	ScratchBuffer createScratchBuffer(VkDeviceSize size)
	{
		ScratchBuffer scratchBuffer{};
		// Buffer and memory
		vk::BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.size = size;
		bufferCreateInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
		scratchBuffer.handle = Core::Context::Get()->GetDevice()->createBuffer(bufferCreateInfo);
		vk::MemoryRequirements memoryRequirements = Core::Context::Get()->GetDevice()->getBufferMemoryRequirements(scratchBuffer.handle);
		vk::MemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
		memoryAllocateFlagsInfo.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
		vk::MemoryAllocateInfo memoryAllocateInfo = {};
		memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = Core::Context::Get()->findMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		Core::Context::Get()->GetDevice()->allocateMemory(&memoryAllocateInfo, nullptr, &scratchBuffer.memory);
		Core::Context::Get()->GetDevice()->bindBufferMemory(scratchBuffer.handle, scratchBuffer.memory, 0);
		// Buffer device address
		vk::BufferDeviceAddressInfoKHR bufferDeviceAddresInfo{};
		bufferDeviceAddresInfo.buffer = scratchBuffer.handle;
		scratchBuffer.deviceAddress = Core::Context::Get()->GetDevice()->getBufferAddressKHR(&bufferDeviceAddresInfo);
		return scratchBuffer;
	}
	void deleteScratchBuffer(ScratchBuffer& scratchBuffer)
	{
		if (scratchBuffer.memory) {
			Core::Context::Get()->GetDevice()->freeMemory(scratchBuffer.memory, nullptr);
		}
		if (scratchBuffer.handle) {
			Core::Context::Get()->GetDevice()->destroyBuffer(scratchBuffer.handle, nullptr);
		}
	}
}