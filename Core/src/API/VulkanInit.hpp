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
		Context();
		~Context();
		static void Init();
		static void Shutdown();
		void SetWindowSurface(void* window);
		void InitDevice();
		void CreateSwapchain(uint32_t width, uint32_t height);
		void CreateSwapchainImages();
		void AcquireNextSwapchainImage();
		void DestroySwapchain();
		void RecreateSwapchain(uint32_t width, uint32_t height);
		void Present();
		void WaitIdle();
		uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
		void allocateBufferMemory(vk::Buffer& buffer, vk::DeviceMemory& mem, vk::MemoryPropertyFlags flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::MemoryAllocateFlags allocInfo = vk::MemoryAllocateFlagBits::eDeviceAddressCaptureReplay);
		static const std::unique_ptr<Context>& Get() { return s_Context; }
		const vk::CommandPool& GetGraphicsCommandPool() { return m_GraphicsCommandPool; }
		const vk::Queue& GetGraphicsQueue() { return m_GraphicsQueue; }
		const vk::Semaphore& GetImageAvailableSemaphore() { return m_ImageAvailableSemaphores[m_Swapchain.currentFrame]; }
		const vk::Semaphore& GetRenderFinishedSemaphore() { return m_RenderFinishedSemaphores[m_Swapchain.currentFrame]; }
		const vk::Fence& GetInFlightFence() { return m_InFlightFences[m_Swapchain.currentFrame]; }
		const vk::Fence& GetImagesInFlightFence() { return m_ImagesInFlight[m_Swapchain.currentFrame]; }
		const vk::PhysicalDevice& GetGPU() { return m_GPU; }
		const vk::UniqueDevice& GetDevice() { return m_Device; }
		const Swapchain& GetSwapchain() { return m_Swapchain; }
		const vk::DescriptorPool& GetDescriptorPool() { return m_DescriptorPool; }
	private:
		static std::unique_ptr<Context> s_Context;
		vk::UniqueInstance m_Instance;
		vk::DispatchLoaderDynamic m_Dispatcher;
		vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> m_DebugMessenger;
		vk::UniqueSurfaceKHR m_Surface;
		vk::PhysicalDevice m_GPU;
		vk::UniqueDevice m_Device;
		vk::Queue m_GraphicsQueue;
		vk::Queue m_TransferQueue;
		vk::Queue m_ComputeQueue;
		vk::CommandPool m_GraphicsCommandPool;
		vk::CommandPool m_TransferCommandPool;
		vk::CommandPool m_ComputeCommandPool;
		vk::DescriptorPool m_DescriptorPool;
		std::vector<const char*> m_Layers;
		std::vector<const char*> m_InstanceExtensions;
		std::vector<const char*> m_DeviceExtensions;
		QueueFamilyIndices m_QueueFamilyIndices;
		Swapchain m_Swapchain;
		std::vector<vk::Semaphore>			m_ImageAvailableSemaphores;
		std::vector<vk::Semaphore>			m_RenderFinishedSemaphores;
		std::vector<vk::Fence>				m_InFlightFences;
		std::vector<vk::Fence>				m_ImagesInFlight;
	};

}