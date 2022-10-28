#include "pch.hpp"
#include "VulkanInit.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Core
{
	vk::UniqueInstance Context::_instance;
	vk::DispatchLoaderDynamic Context::_dispatcher;
	vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> Context::_debugMessenger;
	vk::UniqueSurfaceKHR Context::_surface;
	vk::PhysicalDevice Context::_GPU;
	vk::UniqueDevice Context::_device;
	vk::Queue Context::_graphicsQueue;
	vk::Queue Context::_transferQueue;
	vk::Queue Context::_computeQueue;
	vk::CommandPool Context::_graphicsCommandPool;
	vk::CommandPool Context::_transferCommandPool;
	vk::CommandPool Context::_computeCommandPool;
	vk::DescriptorPool Context::_editorDescriptorPool;
	vk::DescriptorPool Context::_descriptorPool;
	std::vector<const char*> Context::_layers;
	std::vector<const char*> Context::_instanceExtensions;
	std::vector<const char*> Context::_deviceExtensions;
	QueueFamilyIndices Context::_queueFamilyIndices;
	Swapchain Context::_swapchain;
	std::vector<vk::Semaphore>			Context::imageAvailableSemaphores;
	std::vector<vk::Semaphore>			Context::renderFinishedSemaphores;
	std::vector<vk::Fence>				Context::inFlightFences;
	std::vector<vk::Fence>				Context::imagesInFlight;
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
		auto memProperties = _GPU.getMemoryProperties();

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
	void Context::allocateBufferMemory(vk::Buffer& buffer, vk::DeviceMemory& mem)
	{
		vk::MemoryRequirements memoryRequirements = Core::Context::_device->getBufferMemoryRequirements(buffer);
		vk::MemoryAllocateInfo allocInfo;
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = Core::Context::findMemoryType(memoryRequirements.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);

		mem = Core::Context::_device->allocateMemory(allocInfo);
		Core::Context::_device->bindBufferMemory(buffer, mem, 0);
	}
	void Context::Init()
	{
		uint32_t glfwExtensionsCount = 0;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionsCount);
		_instanceExtensions = extensions;
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
			instanceInfo.enabledLayerCount = static_cast<uint32_t>(_layers.size());
			instanceInfo.ppEnabledLayerNames = _layers.data();
			instanceInfo.enabledExtensionCount = static_cast<uint32_t>(_instanceExtensions.size());
			instanceInfo.ppEnabledExtensionNames = _instanceExtensions.data();
			instanceInfo.pApplicationInfo = &appInfo;

			_instance = vk::createInstanceUnique(instanceInfo);
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
			_debugMessenger = _instance->createDebugUtilsMessengerEXTUnique(debugInfo, nullptr, _dispatcher);
#endif
		}
	}
	void Context::Shutdown()
	{
		Context::DestroySwapchain();
		_device->destroyCommandPool(_graphicsCommandPool);
		_device->destroyCommandPool(_transferCommandPool);
		_device->destroyCommandPool(_computeCommandPool);
		_device->destroyDescriptorPool(_editorDescriptorPool);
		_device->destroyDescriptorPool(_descriptorPool);
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			Context::_device->destroySemaphore(imageAvailableSemaphores[i]);
			Context::_device->destroySemaphore(renderFinishedSemaphores[i]);
			Context::_device->destroyFence(inFlightFences[i]);
		}
	}
	void Context::SetWindowSurface(void* window)
	{
		VkSurfaceKHR raw;
		glfwCreateWindowSurface(_instance.get(), (GLFWwindow*)window, nullptr, &raw);
		vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE> _deleter(_instance.get());
		_surface = vk::UniqueSurfaceKHR(vk::SurfaceKHR(raw), _deleter);
	}
	void Context::InitDevice()
	{
		//GPU
		{
			std::vector<vk::PhysicalDevice> _avaibleGPUs = _instance->enumeratePhysicalDevices();
			std::cout << ("Enumerating GPUs...") << std::endl;
			{
#ifdef AZ_DEBUG
				uint8_t gpuIterator = 0;
				for (uint8_t i = 0; i < _avaibleGPUs.size(); i++)
				{
					auto props = _avaibleGPUs[i].getProperties();
					AZ_TRACE("GPU[{0}]:", gpuIterator);
					AZ_TRACE("\tName: {0}", props.deviceName);
					AZ_TRACE("\tType: {0}", vk::to_string(props.deviceType));
					AZ_TRACE("\tDriver Version: {0}.{1}.{2}", VK_VERSION_MAJOR(props.driverVersion), VK_VERSION_MINOR(props.driverVersion), VK_VERSION_PATCH(props.driverVersion));
					gpuIterator++;
				}
#endif
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
				_GPU = GPUsChooseable.rbegin()->second;
			}
			 std::cout << "Selected GPU: " << _GPU.getProperties().deviceName << std::endl;
			 std::cout << "Vulkan API Version " <<
				VK_VERSION_MAJOR(_GPU.getProperties().apiVersion)  << "." <<
				VK_VERSION_MINOR(_GPU.getProperties().apiVersion) << "." <<
				VK_VERSION_PATCH(_GPU.getProperties().apiVersion) << std::endl;
			std::cout << "Getting " << _GPU.getProperties().deviceName << " QueuesFamiliesIndexes:" << std::endl;
			auto queueFamilyProps = _GPU.getQueueFamilyProperties();
			uint8_t min_transfer_score = 255;
			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProps.size()); ++i)
			{
				uint8_t current_transfer_score = 0;

				if (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eGraphics)
				{
					_queueFamilyIndices.graphicsFamily = i;
					++current_transfer_score;
				}
				if (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eCompute)
				{
					_queueFamilyIndices.computeFamily = i;
					++current_transfer_score;
				}
				if (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eTransfer)
				{
					if (current_transfer_score <= min_transfer_score)
					{
						min_transfer_score = current_transfer_score;
						_queueFamilyIndices.transferFamily = i;
					}
				}
			}
			std::cout << "\tGraphics Queue Family Index: "  << _queueFamilyIndices.graphicsFamily	<< std::endl;
			std::cout << "\tCompute Queue  Family Index: "  << _queueFamilyIndices.computeFamily	<< std::endl;
			std::cout << "\tTransfer Queue  Family Index: " <<  _queueFamilyIndices.transferFamily	<< std::endl;

			std::set<uint32_t> uniqueQueueFamilies = { _queueFamilyIndices.graphicsFamily,
			_queueFamilyIndices.transferFamily,
			_queueFamilyIndices.computeFamily };

			_queueFamilyIndices.QueueFamilies = std::vector(uniqueQueueFamilies.begin(), uniqueQueueFamilies.end());

			_queueFamilyIndices.graphicsQueueIndex = _queueFamilyIndices.graphicsFamily;
			_queueFamilyIndices.transferQueueIndex = _queueFamilyIndices.transferFamily;
			_queueFamilyIndices.computeQueueIndex = _queueFamilyIndices.computeFamily;
		}
		//Device
		{
			_deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

			uint32_t index_count = 1;;
			if (!_queueFamilyIndices.doesTransferShareGraphicsQueue())
				index_count++;
			if (!_queueFamilyIndices.doesComputeShareGraphicsQueue())
				index_count++;
			std::vector<uint32_t> indices(index_count);
			uint8_t index = 0;
			indices[index++] = _queueFamilyIndices.graphicsQueueIndex;
			if (!_queueFamilyIndices.doesTransferShareGraphicsQueue())
				indices[index++] = _queueFamilyIndices.transferQueueIndex;
			if (!_queueFamilyIndices.doesComputeShareGraphicsQueue())
				indices[index++] = _queueFamilyIndices.computeQueueIndex;

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
			deviceInfo.enabledLayerCount = static_cast<uint32_t>(_layers.size());
			deviceInfo.ppEnabledLayerNames = _layers.data();
			deviceInfo.enabledExtensionCount = static_cast<uint32_t>(_deviceExtensions.size());
			deviceInfo.ppEnabledExtensionNames = _deviceExtensions.data();
			deviceInfo.pEnabledFeatures = &_GPU.getFeatures();

			_device = _GPU.createDeviceUnique(deviceInfo);

			_graphicsQueue = _device->getQueue(_queueFamilyIndices.graphicsFamily, 0);
			_transferQueue = _device->getQueue(_queueFamilyIndices.transferFamily, 0);
			_computeQueue = _device->getQueue(_queueFamilyIndices.computeFamily, 0);
		}
		//GraphicsCommandPool
		{
			vk::CommandPoolCreateInfo commandPoolInfo;
			commandPoolInfo.sType = vk::StructureType::eCommandPoolCreateInfo;
			commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;		//Might need Fix
			commandPoolInfo.queueFamilyIndex = _queueFamilyIndices.graphicsFamily;
			_graphicsCommandPool = _device->createCommandPool(commandPoolInfo);
		}
		//TransferCommandPool
		{
			vk::CommandPoolCreateInfo commandPoolInfo;
			commandPoolInfo.sType = vk::StructureType::eCommandPoolCreateInfo;
			commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;		//Might need Fix
			commandPoolInfo.queueFamilyIndex = _queueFamilyIndices.transferFamily;
			_transferCommandPool = _device->createCommandPool(commandPoolInfo);
		}
		//TransferCommandPool
		{
			vk::CommandPoolCreateInfo commandPoolInfo;
			commandPoolInfo.sType = vk::StructureType::eCommandPoolCreateInfo;
			commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;		//Might need Fix
			commandPoolInfo.queueFamilyIndex = _queueFamilyIndices.computeFamily;
			_computeCommandPool = _device->createCommandPool(commandPoolInfo);
		}
		//Editor Descriptor Pool
		{
			vk::DescriptorPoolSize pool_sizes[] =
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
			vk::DescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = vk::StructureType::eDescriptorPoolCreateInfo;
			pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
			pool_info.maxSets = 1000 * ((int)(sizeof(pool_sizes) / sizeof(*(pool_sizes))));
			pool_info.poolSizeCount = (uint32_t)((int)(sizeof(pool_sizes) / sizeof(*(pool_sizes))));
			pool_info.pPoolSizes = pool_sizes;
			_editorDescriptorPool = Context::_device->createDescriptorPool(pool_info);
		}
		//Descriptor Pool
		{
			vk::DescriptorPoolSize pool_sizes[] =
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
			vk::DescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = vk::StructureType::eDescriptorPoolCreateInfo;
			pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
			pool_info.maxSets = 1000 * ((int)(sizeof(pool_sizes) / sizeof(*(pool_sizes))));
			pool_info.poolSizeCount = (uint32_t)((int)(sizeof(pool_sizes) / sizeof(*(pool_sizes))));
			pool_info.pPoolSizes = pool_sizes;
			_descriptorPool = Context::_device->createDescriptorPool(pool_info);
		}
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		vk::SemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = vk::StructureType::eSemaphoreCreateInfo;
		vk::FenceCreateInfo renderFenceInfo;
		renderFenceInfo.sType = vk::StructureType::eFenceCreateInfo;
		renderFenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			imageAvailableSemaphores[i] = _device->createSemaphore(semaphoreInfo);
			renderFinishedSemaphores[i] = _device->createSemaphore(semaphoreInfo);
			inFlightFences[i] = _device->createFence(renderFenceInfo);
		}
	}
	////////////////////////////////////////////////////////////////////////////////////Objects-Creation///////////////////////////////////////////////////////////////////
	void Context::CreateSwapchain(uint32_t width, uint32_t height)
	{
		Context::_swapchain.swapchainWidth = width;
		Context::_swapchain.swapchainHeight = height;
		Context::_swapchain.extent =
			vk::Extent2D{ static_cast<uint32_t>(Context::_swapchain.swapchainWidth), static_cast<uint32_t>(Context::_swapchain.swapchainHeight) };

		for (auto mode : Context::_GPU.getSurfacePresentModesKHR(Context::_surface.get()))
		{
			if (mode == vk::PresentModeKHR::eFifo || mode == vk::PresentModeKHR::eMailbox)
				Context::_swapchain.presentMode = mode;
			if (mode == vk::PresentModeKHR::eFifo)
				break;
		}
		vk::SwapchainCreateInfoKHR swapchainInfo;
		swapchainInfo.sType = vk::StructureType::eSwapchainCreateInfoKHR;
		swapchainInfo.surface = *Context::_surface;
		swapchainInfo.minImageCount = Context::_GPU.getSurfaceCapabilitiesKHR(*Context::_surface).minImageCount/* + 1*/;
		swapchainInfo.imageFormat = vk::Format::eB8G8R8A8Unorm;
		swapchainInfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
		swapchainInfo.imageExtent = Context::_swapchain.extent;
		swapchainInfo.imageArrayLayers = 1;
		swapchainInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
		swapchainInfo.imageSharingMode = vk::SharingMode::eExclusive;
		swapchainInfo.queueFamilyIndexCount = 0;
		swapchainInfo.pQueueFamilyIndices = nullptr;
		swapchainInfo.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
		swapchainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		swapchainInfo.presentMode = Context::_swapchain.presentMode;
		swapchainInfo.clipped = true;
		swapchainInfo.oldSwapchain = Context::_swapchain.oldswapchain;

		Context::_swapchain.swapchain = Context::_device->createSwapchainKHR(swapchainInfo);
	}
	void Context::CreateSwapchainImages()
	{
		Context::_swapchain.swapchainImages = Context::_device->getSwapchainImagesKHR(Context::_swapchain.swapchain);
		Context::_swapchain.swapchainImageViews.resize(Context::_swapchain.swapchainImages.size());
		for (size_t i = 0; i < Context::_swapchain.swapchainImages.size(); i++) {
			vk::ImageViewCreateInfo imageViewInfo;
			imageViewInfo.sType = vk::StructureType::eImageViewCreateInfo;
			imageViewInfo.image = Context::_swapchain.swapchainImages[i];
			imageViewInfo.viewType = vk::ImageViewType::e2D;
			imageViewInfo.format = vk::Format::eB8G8R8A8Unorm;
			imageViewInfo.components = vk::ComponentMapping{ vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
			imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			imageViewInfo.subresourceRange.baseMipLevel = 0;
			imageViewInfo.subresourceRange.levelCount = 1;
			imageViewInfo.subresourceRange.baseArrayLayer = 0;
			imageViewInfo.subresourceRange.layerCount = 1;

			Context::_swapchain.swapchainImageViews[i] = Context::_device->createImageView(imageViewInfo);
		}
	}
	void Context::AcquireNextSwapchainImage()
	{
		Context::_device->waitForFences(Context::inFlightFences[Context::_swapchain.currentFrame], true, UINT64_MAX);
		auto acquireResult = Context::_device->acquireNextImageKHR(Context::_swapchain.swapchain, UINT64_MAX, Context::imageAvailableSemaphores[Context::_swapchain.currentFrame]);
		Context::_swapchain.nextSwapchainImage = acquireResult.value;
	}
	void Context::DestroySwapchain()
	{
		for (auto& imageView : Context::_swapchain.swapchainImageViews) {
			Context::_device->destroyImageView(imageView);
		}
		Context::_device->destroySwapchainKHR(Context::_swapchain.swapchain);
	}
	void Context::RecreateSwapchain(uint32_t width, uint32_t height)
	{
		Context::CreateSwapchain(width, height);
		Context::CreateSwapchainImages();
	}
	void Context::Present()
	{
		vk::PresentInfoKHR presentInfo;
		presentInfo.sType = vk::StructureType::ePresentInfoKHR;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &Context::_swapchain.swapchain;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &Context::renderFinishedSemaphores[Context::_swapchain.currentFrame];
		presentInfo.pImageIndices = &Context::_swapchain.nextSwapchainImage;
		Context::_graphicsQueue.presentKHR(presentInfo);
		Context::_swapchain.currentFrame = (Context::_swapchain.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}
	////////////////////////////////////////////////////////////////////////////////////////COMMANDS////////////////////////////////////////////////////////////////////
	void Context::WaitIdle()
	{
		Context::_device->waitIdle();
	}
	static uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
		auto memProperties = Context::_GPU.getMemoryProperties();

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		return -1;
	}
}