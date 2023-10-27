#pragma once
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#define MAX_FRAMES_IN_FLIGHT 2

namespace Core
{
	struct ScratchBuffer
	{
		uint64_t deviceAddress = 0;
		vk::Buffer handle = VK_NULL_HANDLE;
		vk::DeviceMemory memory = VK_NULL_HANDLE;
	};
	ScratchBuffer createScratchBuffer(VkDeviceSize size);
	void deleteScratchBuffer(ScratchBuffer& scratchBuffer);
	struct QueueFamilyIndices
	{
	public:
		bool isComplete()
		{
			return graphicsFamily != -1 &&
				transferFamily != -1 &&
				computeFamily != -1;
		}
		bool doesTransferShareGraphicsQueue()
		{
			return transferQueueIndex == graphicsQueueIndex;
		}
		bool doesComputeShareGraphicsQueue()
		{
			return computeQueueIndex == graphicsQueueIndex;
		}
	public:
		uint32_t graphicsFamily = -1;
		uint32_t transferFamily = -1;
		uint32_t computeFamily = -1;

		uint32_t graphicsQueueIndex = -1;
		uint32_t transferQueueIndex = -1;
		uint32_t computeQueueIndex = -1;

		std::vector<uint32_t> QueueFamilies = {};
	};
	struct Swapchain
	{
		vk::SwapchainKHR swapchain;
		vk::SwapchainKHR oldswapchain;
		vk::Extent2D extent;
		std::vector<vk::Image> swapchainImages;
		std::vector<vk::ImageView> swapchainImageViews;
		uint32_t currentFrame = 0;
		uint32_t nextSwapchainImage = 0;
		int swapchainWidth, swapchainHeight;
		vk::PresentModeKHR presentMode;
	};
	struct Context
	{
		static void Init();
		static void Shutdown();
		static void SetWindowSurface(void* window);
		static void InitDevice();
		static void CreateSwapchain(uint32_t width, uint32_t height);
		static void CreateSwapchainImages();
		static void AcquireNextSwapchainImage();
		static void DestroySwapchain();
		static void RecreateSwapchain(uint32_t width, uint32_t height);
		static void Present();
		static void WaitIdle();
		static uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
		static void allocateBufferMemory(vk::Buffer& buffer, vk::DeviceMemory& mem, vk::MemoryPropertyFlags flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::MemoryAllocateFlags allocInfo = vk::MemoryAllocateFlagBits::eDeviceAddressCaptureReplay);
		static vk::UniqueInstance _instance;
		static vk::DispatchLoaderDynamic _dispatcher;
		static vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> _debugMessenger;
		static vk::UniqueSurfaceKHR _surface;
		static vk::PhysicalDevice _GPU;
		static vk::UniqueDevice _device;
		static vk::Queue _graphicsQueue;
		static vk::Queue _transferQueue;
		static vk::Queue _computeQueue;
		static vk::CommandPool _graphicsCommandPool;
		static vk::CommandPool _transferCommandPool;
		static vk::CommandPool _computeCommandPool;
		static vk::DescriptorPool _descriptorPool;
		static std::vector<const char*> _layers;
		static std::vector<const char*> _instanceExtensions;
		static std::vector<const char*> _deviceExtensions;
		static QueueFamilyIndices _queueFamilyIndices;
		static Swapchain _swapchain;
		static std::vector<vk::Semaphore>			imageAvailableSemaphores;
		static std::vector<vk::Semaphore>			renderFinishedSemaphores;
		static std::vector<vk::Fence>				inFlightFences;
		static std::vector<vk::Fence>				imagesInFlight;
	};

}