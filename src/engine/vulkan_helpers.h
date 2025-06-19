#ifndef TRIEX_VULKAN_HELPERS
#define TRIEX_VULKAN_HELPERS

#include <stdbool.h>

#include "vulkan/vulkan.h"
int getNeededQueueFamilyIndex(VkQueueFlags flags);
bool findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, uint32_t* index);
VkCommandBuffer beginSingleTimeCommands();
void endSingleTimeCommands(VkCommandBuffer commandBuffer);

typedef struct {
    float r;
    float g;
    float b;
    float a;
} Color;

typedef struct{
    VkImageView colorAttachment;
    Color clearColor;
    VkImageView depthAttachment;
} BeginRenderingEX;

void vkCmdBeginRenderingEX(VkCommandBuffer commandBuffer, BeginRenderingEX args);

VkDeviceAddress vkGetBufferDeviceAddressEX(VkBuffer buffer);

void beginDrawing();
void endDrawing();
VkImageView getSwapchainImageView();


#endif