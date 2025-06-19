#ifndef TRIEX_VULKAN_GLOBALS
#define TRIEX_VULKAN_GLOBALS

#include "vulkan/vulkan.h"

extern VkInstance instance;
extern VkPhysicalDevice physicalDevice;
extern VkDevice device;
extern VkPhysicalDeviceLimits physicalDeviceLimits;
extern VkSurfaceKHR surface;
extern VkSwapchainKHR swapchain;
extern VkFormat swapchainImageFormat;
extern VkExtent2D swapchainExtent;
extern VkQueue graphicsQueue;
extern VkQueue presentQueue;
extern VkPhysicalDeviceMemoryProperties memoryProperties;

typedef struct {
    VkImage* items;
    uint32_t count;
    size_t capacity;
} VkImages;

typedef struct {
    VkImageView* items;
    uint32_t count;
    size_t capacity;
} VkImageViews;

extern VkImages swapchainImages;
extern VkImageViews swapchainImageViews;

typedef struct{
    VkQueueFamilyProperties* items;
    uint32_t count;
    size_t capacity;
} MultipleVkQueueFamilyProperties;
extern MultipleVkQueueFamilyProperties multipleQueueFamilyProperties;

extern VkCommandPool commandPool;
extern VkCommandBuffer cmd;

extern VkSemaphore imageAvailableSemaphore;
extern VkSemaphore renderFinishedSemaphore;
extern VkFence inFlightFence;

static const uint32_t k_max_bindless_resources = 16536;
extern VkDescriptorPool descriptorPool;

extern VkSampler samplerLinear;
extern VkSampler samplerNearest;

#endif