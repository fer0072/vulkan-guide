
#pragma once 

#include <vulkan/vulkan.h>

namespace vkUtils {

	void imageLayoutTransition(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

	void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

	void generateImageMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
} // namespace vkUtils
