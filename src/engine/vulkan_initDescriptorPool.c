#include <stdio.h>
#include <stdbool.h>

#define NOB_STRIP_PREFIX
#include "nob.h"

#include "vulkan/vulkan.h"

#include "vulkan_globals.h"
#include "vulkan_initDescriptorPool.h"

VkDescriptorPool descriptorPool;

// Took from https://jorenjoestar.github.io/post/vulkan_bindless_texture/ (slightly modified)
bool initDescriptorPool(){

    VkDescriptorPoolSize descriptorPoolSize = {0};
    descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorPoolSize.descriptorCount = k_max_bindless_resources;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {0};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = NULL;
    // Update after bind is needed here, for each binding and in the descriptor set layout creation.
    descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    descriptorPoolCreateInfo.maxSets = k_max_bindless_resources;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
    VkResult result = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &descriptorPool);
    if(result != VK_SUCCESS){
        printf("ERROR: Couldn't create descriptor pool\n");
        return false;
    }

    return true;
}