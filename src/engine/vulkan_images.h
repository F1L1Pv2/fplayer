#ifndef TRIEX_VULKAN_IMAGES
#define TRIEX_VULKAN_IMAGES

#include <stdbool.h>
#include <vulkan/vulkan.h>

bool createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView* out);
bool createImage(size_t width, size_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperties, VkImage* image, VkDeviceMemory* imageMemory);
bool sendDataToImage(VkImage image, char* data, size_t width, size_t stride, size_t height);

#endif