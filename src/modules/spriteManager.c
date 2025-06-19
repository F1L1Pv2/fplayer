#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define NOB_STRIP_PREFIX
#include "nob.h"

#include "vulkan/vulkan.h"
#include "engine/vulkan_globals.h"
#include "engine/vulkan_buffer.h"

#include "spriteManager.h"

typedef struct {
    SpriteDrawCommand* items;
    size_t count;
    size_t capacity;
} SpriteDrawCommands;

static SpriteDrawCommands spriteDrawQueue = {0};

VkBuffer spriteDrawBuffer;
VkDeviceMemory spriteDrawMemory;

bool initSpriteManager(){
    if(!createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,sizeof(SpriteDrawCommand)*MAX_SPRITE_COUNT,&spriteDrawBuffer,&spriteDrawMemory)) return false;
    return true;
}

void drawSprite(SpriteDrawCommand cmd){
    if(spriteDrawQueue.count < MAX_SPRITE_COUNT){
        if(cmd.size.x == 0 && cmd.size.y == 0){
            cmd.size.x = 1;
            cmd.size.y = 1;
        }
        da_append(&spriteDrawQueue, cmd);
    }
}

void renderSprites(){
    // its assumes that everything is attached correctly

    if(spriteDrawQueue.count > 0){
        transferDataToMemory(spriteDrawMemory,spriteDrawQueue.items,0,sizeof(spriteDrawQueue.items[0])*spriteDrawQueue.count);
        vkCmdDraw(cmd,6,spriteDrawQueue.count,0,0);
        spriteDrawQueue.count = 0;
    }

}