
#pragma once 

#include <vulkan/vulkan.h>

namespace vkUtils {

	void imageLayoutTransition(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkImageAspectFlags aspectMask = 0, uint32_t baseMiplevel = 0u, uint32_t levelCount = 1u);

	void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

	void generateImageMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
} // namespace vkUtils
