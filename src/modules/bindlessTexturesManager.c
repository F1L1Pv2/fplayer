#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define NOB_STRIP_PREFIX
#include "nob.h"

#include "vulkan/vulkan.h"
#include "engine/vulkan_globals.h"
#include "engine/vulkan_images.h"

#include "bindlessTexturesManager.h"
#include "3rdparty/stb_image.h"

VkDescriptorSetLayout bindlessDescriptorSetLayout;
VkDescriptorSet bindlessDescriptorSet;

bool initBindlessDescriptorSet(){
    VkDescriptorBindingFlags descriptorBindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {0};
    descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorSetLayoutBinding.descriptorCount = k_max_bindless_resources;
    descriptorSetLayoutBinding.binding = 0;
    descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
    descriptorSetLayoutBinding.pImmutableSamplers = NULL;

    VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {0};
    descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    descriptorSetLayoutBindingFlagsCreateInfo.pNext = NULL;
    descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = 1;
    descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = &descriptorBindingFlags;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {0};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;
    descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;

    VkResult result = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &bindlessDescriptorSetLayout);
    if(result != VK_SUCCESS){
        printf("ERROR: Couldn't create descriptor set layout\n");
        return false;
    }

    VkDescriptorSetVariableDescriptorCountAllocateInfo count_info = {0};
    count_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    count_info.descriptorSetCount = 1;
    uint32_t max_binding = k_max_bindless_resources - 1;
    count_info.pDescriptorCounts = &max_binding;

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {0}; 
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = &count_info;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &bindlessDescriptorSetLayout;

    result = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &bindlessDescriptorSet);
    if(result != VK_SUCCESS){
        printf("ERROR: Couldn't allocate descriptor set\n");
        return false;
    }

    return true;
}

typedef struct {
    Texture* items;
    size_t count;
    size_t capacity;
} Textures;

typedef struct {
    VkDescriptorImageInfo* items;
    size_t count;
    size_t capacity;
} VkDescriptorImageInfos;

Textures bindlessTextures = {0};

bool initBindlessTextures(File_Paths paths){
    if(!initBindlessDescriptorSet()) return false;

    if(paths.count == 0) return true;
    VkDescriptorImageInfos descriptorImageInfos = {0};

    for(int i = 0; i < paths.count; i++){
        Texture texture = {0};
        texture.name = (char*)paths.items[i];

        char* data = (char*)stbi_load(texture.name,(int*)&texture.width,(int*)&texture.height, NULL, 4);
        if(data == NULL){
            printf("ERROR: Couldn't load %s image\n", texture.name);
            return false;
        }
    
        if(!createImage(texture.width,texture.height,VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR,VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &texture.image, &texture.memory)){
            return false;
        }
    
        if(!sendDataToImage(texture.image,data,texture.width,texture.width*sizeof(uint32_t),texture.height)){
            return false;
        }
    
        if(!createImageView(texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, &texture.imageView)){
            return false;
        }

        da_append(&bindlessTextures, texture);
    
        VkDescriptorImageInfo descriptorImageInfo = {0};
        descriptorImageInfo.sampler = samplerLinear;
        descriptorImageInfo.imageView = texture.imageView;
        descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        da_append(&descriptorImageInfos, descriptorImageInfo);
    }

    VkWriteDescriptorSet writeDescriptorSet = {0};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext = NULL;
    writeDescriptorSet.dstSet = bindlessDescriptorSet;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.dstArrayElement = 0;
    writeDescriptorSet.descriptorCount = descriptorImageInfos.count;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pImageInfo = descriptorImageInfos.items;

    vkUpdateDescriptorSets(device,1,&writeDescriptorSet,0,NULL);

    da_free(descriptorImageInfos);

    return true;
}

bool addBindlessTexture(char* name, char* data, size_t width, size_t height){
    if(data == NULL) return false;
    
    Texture texture = (Texture){
        .width = width,
        .height = height,
        .name = name,
    };

    if(!createImage(texture.width,texture.height,VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR,VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &texture.image, &texture.memory)){
        return false;
    }
    
    if(!sendDataToImage(texture.image,data,texture.width,texture.width*sizeof(uint32_t),texture.height)){
        return false;
    }
    
    if(!createImageView(texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, &texture.imageView)){
        return false;
    }

    da_append(&bindlessTextures, texture);
    
    VkDescriptorImageInfo descriptorImageInfo = {0};
    descriptorImageInfo.sampler = samplerLinear;
    descriptorImageInfo.imageView = texture.imageView;
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writeDescriptorSet = {0};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext = NULL;
    writeDescriptorSet.dstSet = bindlessDescriptorSet;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.dstArrayElement = bindlessTextures.count - 1;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pImageInfo = &descriptorImageInfo;

    vkUpdateDescriptorSets(device,1,&writeDescriptorSet,0,NULL);
    
    return true;
}

void addBindlessTextureRaw(Texture texture){
    da_append(&bindlessTextures, texture);
    
    VkDescriptorImageInfo descriptorImageInfo = {0};
    descriptorImageInfo.sampler = samplerLinear;
    descriptorImageInfo.imageView = texture.imageView;
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writeDescriptorSet = {0};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext = NULL;
    writeDescriptorSet.dstSet = bindlessDescriptorSet;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.dstArrayElement = bindlessTextures.count - 1;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pImageInfo = &descriptorImageInfo;

    vkUpdateDescriptorSets(device,1,&writeDescriptorSet,0,NULL);
}

bool addBindlessTextureFromDisk(char* name){
    int width, height;
    char* data = (char*)stbi_load(name,&width,&height, NULL, 4);
    if(data == NULL){
        printf("ERROR: Couldn't load %s image\n", name);
        return false;
    }

    return addBindlessTexture(name,data,width,height);
}

int getTextureID(char* name){
    for(int i = 0; i < bindlessTextures.count; i++){
        if(strcmp(bindlessTextures.items[i].name, name) == 0){
            return i + 1;
        }
    }

    return 0;
}