#ifndef MODULES_BINDLESS_TEXTURES_MANAGER
#define MODULES_BINDLESS_TEXTURES_MANAGER

#include <stdbool.h>

#define NOB_STRIP_PREFIX
#include "nob.h"

#include "vulkan/vulkan.h"

typedef struct{
    char* name;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView imageView;
    size_t width;
    size_t height;
} Texture;

bool initBindlessTextures(File_Paths paths);
bool addBindlessTextureFromDisk(char* name);
bool addBindlessTexture(char* name, char* data, size_t width, size_t height);
void addBindlessTextureRaw(Texture texture);
int getTextureID(char* name);

extern VkDescriptorSetLayout bindlessDescriptorSetLayout;
extern VkDescriptorSet bindlessDescriptorSet;

#endif