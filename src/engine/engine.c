#include <stdio.h>
#include <stdint.h>

#define NOB_STRIP_PREFIX
#include "nob.h"

#include "vulkan_globals.h"
#include "vulkan_initialize.h"
#include "vulkan_getDevice.h"
#include "vulkan_createSurface.h"
#include "vulkan_initSwapchain.h"
#include "vulkan_initCommandPool.h"
#include "vulkan_initCommandBuffer.h"
#include "vulkan_synchro.h"
#include "vulkan_initDescriptorPool.h"
#include "vulkan_initSamplers.h"
#include "vulkan_helpers.h"
#include "platform.h"
#include "app.h"

bool platform_resize_window_callback(){
    if(!swapchain) return true;
    VkResult result = vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    if(result != VK_SUCCESS){
        printf("ERROR: Couldn't wait for fences\n");
        return false;
    }

    for(int i = 0; i < swapchainImageViews.count; i++){
        vkDestroyImageView(device,swapchainImageViews.items[i],NULL);
    }
    swapchainImages.count = 0;
    swapchainImageViews.count = 0;
    vkDestroySwapchainKHR(device, swapchain, NULL);

    if(!initSwapchain()) return false;
    if(!afterResize()) return false;

    return true;
}

static uint64_t oldTime;
static size_t targetFPS;
static float frameDuration;

bool engineInit(char* title, size_t width, size_t height){
#ifndef _WIN32
    setvbuf(stdout, NULL, _IONBF, 0);

#endif
    platform_create_window(title,width,height);

    if(!initialize_vulkan()) return false;
    if(!createSurface()) return false;
    if(!getDevice()) return false;
    if(!initSwapchain()) return false;
    if(!initCommandPool()) return false;
    if(!initCommandBuffer()) return false;
    if(!createAllNeededSyncrhonizationObjects()) return false;
    if(!initDescriptorPool()) return false;
    if(!initSamplers()) return false;

    uint64_t oldTime = platform_get_time();

    targetFPS = 120;
    frameDuration = 1.0f / targetFPS;

    return true;
}

int engineStart(){
    while(platform_still_running()){
        if(!platform_window_handle_events()) {
            printf("ERROR: Couldn't handle window events\n");
            return 1;
        }

        uint64_t time = platform_get_time();
        float deltaTime = (float)(time - oldTime) / 1000.0f;
        if(deltaTime > frameDuration) deltaTime = frameDuration;
        oldTime = time;

        if(!update(deltaTime)){
            printf("ERROR: Couldn't update frame\n");
            return 1;   
        }
        
        beginDrawing();
        if(!draw()) {
            printf("ERROR: Couldn't draw frame\n");
            return 1;
        }
        endDrawing();

        uint64_t frameTook = platform_get_time() - time;
        if(((float)(frameTook)/1000.0f) < frameDuration) platform_sleep(frameDuration*1000.0f - frameTook);
    }

    return 0;
}
