// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

namespace vkInit {
	//> init_cmd
	VkCommandPoolCreateInfo commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

	VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1);
	//< init_cmd

	VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);

	VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);

	VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);

	VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

	VkBufferMemoryBarrier bufferMemoryBarrier(VkBuffer buffer, uint32_t queueFamilyIndex, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);

	VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);

	VkPresentInfoKHR presentInfo();

	VkRenderingAttachmentInfo colorAttachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);

	VkRenderingAttachmentInfo depthAttachmentInfo(VkImageView view, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);

	VkRenderingInfo renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment);

	VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask);

	VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);

	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(VkDescriptorSetLayoutBinding* bindings, uint32_t bindingCount);

	VkWriteDescriptorSet writeDescriptorSetImage(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding);

	VkWriteDescriptorSet writeDescriptorSetBuffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding);

	VkDescriptorBufferInfo descriptorBufferInfo(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

	VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

	VkImageViewCreateInfo imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo();

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char* entry = "main");

}// namespace vkInit
