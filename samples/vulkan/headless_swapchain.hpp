#pragma once

#include <assert.h>
#include <stdint.h>
#include <vector>

#include <vulkan/vulkan.h>
#include "vulkantools.h"

typedef struct _SwapChainBuffers {
	VkImage image;
	VkImageView view;
} SwapChainBuffer;

class VulkanSwapChain
{
private:
	static const uint32_t headlessImageCount = 3;

	VkInstance instance = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	std::vector<VkDeviceMemory> memories;
	uint32_t nextImage = 0;

	uint32_t memoryType(uint32_t typeBits, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
		{
			if ((typeBits & (1u << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		return 0;
	}

	VkResult submitEmpty(VkQueue target, VkSemaphore wait, VkSemaphore signal)
	{
		VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = wait != VK_NULL_HANDLE ? 1 : 0;
		submitInfo.pWaitSemaphores = &wait;
		submitInfo.pWaitDstStageMask = &stage;
		submitInfo.signalSemaphoreCount = signal != VK_NULL_HANDLE ? 1 : 0;
		submitInfo.pSignalSemaphores = &signal;
		return vkQueueSubmit(target, 1, &submitInfo, VK_NULL_HANDLE);
	}

public:
	VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
	VkColorSpaceKHR colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;

	uint32_t imageCount = 0;
	std::vector<VkImage> images;
	std::vector<SwapChainBuffer> buffers;

	uint32_t queueNodeIndex = UINT32_MAX;

	void initSurface()
	{
		uint32_t queueCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
		std::vector<VkQueueFamilyProperties> queueProps(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());
		for (uint32_t i = 0; i < queueCount; i++)
		{
			if (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				queueNodeIndex = i;
				break;
			}
		}
		if (queueNodeIndex == UINT32_MAX)
		{
			vkTools::exitFatal("No graphics queue", "Fatal error");
		}
		vkGetDeviceQueue(device, queueNodeIndex, 0, &queue);
	}

	void connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		this->instance = instance;
		this->physicalDevice = physicalDevice;
		this->device = device;
	}

	void create(VkCommandBuffer cmdBuffer, uint32_t *width, uint32_t *height)
	{
		cleanup();
		imageCount = headlessImageCount;
		images.resize(imageCount);
		buffers.resize(imageCount);
		memories.resize(imageCount);
		for (uint32_t i = 0; i < imageCount; i++)
		{
			VkImageCreateInfo imageInfo = {};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.format = colorFormat;
			imageInfo.extent = { *width, *height, 1 };
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			VK_CHECK_RESULT(vkCreateImage(device, &imageInfo, nullptr, &images[i]));

			VkMemoryRequirements requirements;
			vkGetImageMemoryRequirements(device, images[i], &requirements);
			VkMemoryAllocateInfo allocateInfo = {};
			allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocateInfo.allocationSize = requirements.size;
			allocateInfo.memoryTypeIndex = memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &allocateInfo, nullptr, &memories[i]));
			VK_CHECK_RESULT(vkBindImageMemory(device, images[i], memories[i], 0));

			vkTools::setImageLayout(
				cmdBuffer,
				images[i],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

			VkImageViewCreateInfo viewInfo = {};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = images[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = colorFormat;
			viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			buffers[i].image = images[i];
			VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &buffers[i].view));
		}
		nextImage = 0;
	}

	VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t *currentBuffer)
	{
		*currentBuffer = nextImage;
		nextImage = (nextImage + 1) % imageCount;
		return submitEmpty(queue, VK_NULL_HANDLE, presentCompleteSemaphore);
	}

	VkResult queuePresent(VkQueue queue, uint32_t currentBuffer)
	{
		return VK_SUCCESS;
	}

	VkResult queuePresent(VkQueue queue, uint32_t currentBuffer, VkSemaphore waitSemaphore)
	{
		if (waitSemaphore == VK_NULL_HANDLE)
		{
			return VK_SUCCESS;
		}
		return submitEmpty(queue, waitSemaphore, VK_NULL_HANDLE);
	}

	void cleanup()
	{
		for (uint32_t i = 0; i < imageCount; i++)
		{
			vkDestroyImageView(device, buffers[i].view, nullptr);
			vkDestroyImage(device, images[i], nullptr);
			vkFreeMemory(device, memories[i], nullptr);
		}
		imageCount = 0;
	}
};
