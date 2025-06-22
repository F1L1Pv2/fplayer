#include <stdio.h>
#include <stdbool.h>

#define NOB_STRIP_PREFIX
#include "nob.h"

#include "vulkan/vulkan.h"

#include "engine/engine.h"
#include "engine/app.h"
#include "engine/vulkan_globals.h"
#include "engine/vulkan_createGraphicPipelines.h"
#include "engine/vulkan_compileShader.h"
#include "engine/vulkan_helpers.h"
#include "engine/vulkan_buffer.h"
#include "engine/vulkan_images.h"
#include "engine/input.h"

#include "math.h"
#include "modules/gmath.h"
#include "modules/bindlessTexturesManager.h"
#include "modules/spriteManager.h"
#include "modules/font_freetype.h"
#include "ffmpeg_video.h"
#include "ffmpeg_audio.h"
#include "gui_helpers.h"
#include "sound_engine.h"
#include <stdatomic.h>

typedef struct{
    mat4 projView;
    VkDeviceAddress SpriteDrawBufferPtr;
} PushConstants;

typedef struct{
    mat4 projView;
    mat4 model;
} PushConstantsPreview;

static PushConstants pcs;
static PushConstantsPreview pcsPreview;

bool afterResize(){
    mat4 ortho = ortho2D(swapchainExtent.width, swapchainExtent.height);
    mat4 projView = mat4mul(&ortho, &(mat4){
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        -((float)swapchainExtent.width)/2,-((float)swapchainExtent.height)/2,0,1,
    });

    pcs.projView = projView;
    pcsPreview.projView = projView;

    return true;
}

static VkPipeline pipeline;
static VkPipelineLayout pipelineLayout;

static VkPipeline pipelinePreview;
static VkPipelineLayout pipelineLayoutPreview;
static VkDescriptorSet descriptorSet;
static VkDescriptorSetLayout descriptorSetLayout;

GlyphAtlas atlas = {0};

#include "engine/platform.h"

uint64_t TIMER;
uint64_t TIMER_TOTAL;
#define CHECK_TIMER(thing) do {uint64_t newTimer = platform_get_time();printf("%s: took %.2fs\n", (thing), (float)(newTimer - TIMER) / 1000.0f);TIMER = newTimer;} while(0)
#define CHECK_TIMER_TOTAL(thing) do {uint64_t newTimer = platform_get_time();printf("%s: took %.2fs\n", (thing), (float)(newTimer - TIMER_TOTAL) / 1000.0f);TIMER_TOTAL = newTimer;} while(0)

Video video = {0};
Frame videoFrame = {0};
void* videoMapped = NULL;
size_t videoVulkanStride = 0;
Audio audio = {0};
bool audioInMedia = false;


int main(int argc, char** argv){
    if(argc < 2){
        printf("you need to provide filename\n");
        return 1;
    }
    TIMER = platform_get_time();
    TIMER_TOTAL = TIMER;
    if(!engineInit("Fplayer", 640,480)) return 1;
    CHECK_TIMER("init engine");

    {
        afterResize();

        // ------------------ sprite manager initialization ------------------

        File_Paths initialTextures = {0};
        if(!initBindlessTextures(initialTextures)) return 1;

        String_Builder sb = {0};
        nob_read_entire_file("assets/shaders/compiled/sprite.vert.spv",&sb);
        sb_append_null(&sb);
        
        VkShaderModule vertexShader;
        if(!compileShaderFromBinary((uint32_t*)sb.items,sb.count-1,&vertexShader)) return false;
        
        sb.count = 0;
        nob_read_entire_file("assets/shaders/compiled/sprite.frag.spv",&sb);
        sb_append_null(&sb);
        
        VkShaderModule fragmentShader;
        if(!compileShaderFromBinary((uint32_t*)sb.items,sb.count-1,&fragmentShader)) return false;
        
        if(!createGraphicPipeline((CreateGraphicsPipelineARGS){
            .vertexShader = vertexShader,
            .fragmentShader = fragmentShader,
            .pushConstantsSize = sizeof(PushConstants),
            .pipelineOUT = &pipeline, 
            .pipelineLayoutOUT = &pipelineLayout,
            .descriptorSetLayoutCount = 1,
            .descriptorSetLayouts = &bindlessDescriptorSetLayout,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        })) return false;
        
        if(!initSpriteManager()) return 1;
        pcs.SpriteDrawBufferPtr = vkGetBufferDeviceAddressEX(spriteDrawBuffer);

        CHECK_TIMER("init sprite");

        // ------------------          INIT SDF            ------------------

        VkImage fontImage = NULL;
        VkDeviceMemory fontMemory = NULL;
        VkImageView fontImageView = NULL;

        if(!GetFontSDFAtlas("assets/font/VictorMono-Regular.ttf",&fontImage, &fontMemory, &fontImageView, &atlas)) return false;

        addBindlessTextureRaw((Texture){
            .name = "font",
            .width = atlas.width,
            .height = atlas.height,
            .image = fontImage,
            .memory = fontMemory,
            .imageView = fontImageView,
        });

        CHECK_TIMER("init sdf");

        // ------------------ video preview initialization ------------------

        sb.count = 0;
        nob_read_entire_file("assets/shaders/compiled/normal_texture.vert.spv",&sb);
        sb_append_null(&sb);
        
        if(!compileShaderFromBinary((uint32_t*)sb.items,sb.count-1,&vertexShader)) return false;
        
        sb.count = 0;
        nob_read_entire_file("assets/shaders/compiled/normal_texture.frag.spv",&sb);
        sb_append_null(&sb);
        
        if(!compileShaderFromBinary((uint32_t*)sb.items,sb.count-1,&fragmentShader)) return false;

        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {0};
        descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.binding = 0;
        descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {0};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount  = 1;
        descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;

        if(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayout) != VK_SUCCESS){
            printf("ERROR\n");
            return 1;
        }

        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {0};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.descriptorPool = descriptorPool;
        descriptorSetAllocateInfo.descriptorSetCount = 1;
        descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

        vkAllocateDescriptorSets(device,&descriptorSetAllocateInfo, &descriptorSet);
        
        if(!createGraphicPipeline((CreateGraphicsPipelineARGS){
            .vertexShader = vertexShader,
            .fragmentShader = fragmentShader,
            .pushConstantsSize = sizeof(PushConstantsPreview),
            .pipelineOUT = &pipelinePreview, 
            .pipelineLayoutOUT = &pipelineLayoutPreview,
            .descriptorSetLayoutCount = 1,
            .descriptorSetLayouts = &descriptorSetLayout,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        })) return false;

        da_free(sb);

        VkImage image;
        VkImageView imageView;
        VkDeviceMemory imageMemory;

        if(!ffmpegVideoInit(argv[1], &video)) {
            printf("Couldn't load video\n");
            return 1;
        }

        if(!ffmpegVideoGetFrame(&video,&videoFrame)){
            printf("Couldn't get video frame\n");
            return 1;
        }

        if(!createImage(videoFrame.width,videoFrame.height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &image,&imageMemory)){
            printf("Couldn't create video image\n");
            return 1;
        }

        if(!sendDataToImage(image,videoFrame.data,videoFrame.width, videoFrame.width*sizeof(uint32_t), videoFrame.height)){
            printf("Couldn't send frame data to video image\n");
            return 1;
        }

        if(!createImageView(image,VK_FORMAT_R8G8B8A8_UNORM, 
                        VK_IMAGE_ASPECT_COLOR_BIT, &imageView)){
            printf("Couldn't create video image view\n");
            return 1;
        }

        VkImageSubresource subresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .arrayLayer = 0
        };

        VkSubresourceLayout layout;
        vkGetImageSubresourceLayout(device, image, &subresource, &layout);
        videoVulkanStride = layout.rowPitch;

        vkMapMemory(device,imageMemory, 0, videoVulkanStride*videoFrame.height, 0, &videoMapped);

        VkDescriptorImageInfo descriptorImageInfo = {0};
        descriptorImageInfo.sampler = samplerLinear;
        descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        descriptorImageInfo.imageView = imageView;

        VkWriteDescriptorSet writeDescriptorSet = {0};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDescriptorSet.dstSet = descriptorSet;
        writeDescriptorSet.dstBinding = 0;
        writeDescriptorSet.dstArrayElement = 0;
        writeDescriptorSet.pImageInfo = &descriptorImageInfo;

        vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);

        CHECK_TIMER("init video");

        if(ffmpegAudioInit(argv[1], &audio)){
            audioInMedia = true;
        }
    }

    CHECK_TIMER_TOTAL("init");
    return engineStart();
}

float time;

atomic_bool playing = true;
bool fullscreen = false;

float soundVolume = 1.0f;

bool update(float deltaTime){
    if(audioInMedia){
        time = soundEngineGetTime();
    }else{
        if(playing) time += deltaTime;
    }

    float newScroll = soundVolume + (float)input.scroll/120*deltaTime;
    if(newScroll < 0) newScroll = 0.0;
    if(newScroll > 2) newScroll = 2.0;
    if(newScroll != soundVolume){
        soundEngineSetVolume(newScroll);
        soundVolume = newScroll;
    }

    if(input.keys[KEY_SPACE].justPressed) playing = !playing;
    if(input.keys[KEY_F11].justPressed) {
        fullscreen = !fullscreen;
        if(fullscreen){
            platform_enable_fullscreen();
        }else{
            platform_disable_fullscreen();
        }
    }

    float time_before_seek = time;

    if (input.keys[KEY_RIGHT].isDown && input.keys[KEY_SHIFT].isDown) {
        time += 1.0f / video.frameRate;
    } else if (input.keys[KEY_RIGHT].isDown && !input.keys[KEY_SHIFT].isDown) {
        time += 1.0f;
    }

    if (input.keys[KEY_LEFT].isDown  && input.keys[KEY_SHIFT].isDown) {
        time -= 1.0f / video.frameRate;
    }else if (input.keys[KEY_LEFT].isDown && !input.keys[KEY_SHIFT].isDown) {
        time -= 1.0f;
    }

    if (time > video.duration) time = video.duration;
    if (time < 0) time = 0;

    if (time != time_before_seek && !playing) {
        if(!ffmpegVideoSeek(&video, &videoFrame, time)) return false;
        if(audioInMedia) {
            ffmpegAudioSeek(&audio, time);
            soundEngineSetTime(time);
        }
        if(ffmpegVideoGetFrame(&video,&videoFrame)){
            for(int i = 0; i < videoFrame.height; i++){
                memcpy(
                    videoMapped + videoVulkanStride*i,
                    videoFrame.data + videoFrame.width*sizeof(uint32_t)*i,
                    videoFrame.width *sizeof(uint32_t)
                );
            }
        }
    }


    Rect timelineRect = (Rect){
        .width = swapchainExtent.width,
        .height = min(((float)swapchainExtent.height / 4), 200.0f)
    };

    Rect previewPos = (Rect){
        .width = swapchainExtent.width,
        .height = fullscreen ? swapchainExtent.height : swapchainExtent.height - timelineRect.height,
    };

    timelineRect.y = previewPos.y + previewPos.height;

    Rect previewRect = fitRectangle(previewPos, videoFrame.width, videoFrame.height);
    
    pcsPreview.model = (mat4){
        previewRect.width,0,0,0,
        0,previewRect.height,0,0,
        0,0,1,0,
        previewRect.x,previewRect.y,0,1,
    };

    if(playing){
        bool didSmth = false;
        while(time > videoFrame.frameTime){
            if(ffmpegVideoGetFrame(&video,&videoFrame)){
                didSmth = true;
            }else{
                break;
            }
        }

        if(didSmth){
            for(int i = 0; i < videoFrame.height; i++){
                memcpy(
                    videoMapped + videoVulkanStride*i,
                    videoFrame.data + videoFrame.width*sizeof(uint32_t)*i,
                    videoFrame.width *sizeof(uint32_t)
                );
            }
        }
    }

    if(time >= video.duration){
        time = 0;
        if(!ffmpegVideoSeek(&video, &videoFrame,time)) return false;
        if(audioInMedia) {
            ffmpegAudioSeek(&audio, time);
            soundEngineSetTime(time);
        }
    }

    if(pointInsideRect(input.mouse_x, input.mouse_y, timelineRect) && input.keys[KEY_MOUSE_LEFT].justPressed){
        time = ((float)input.mouse_x - timelineRect.x) * video.duration / timelineRect.width;
        if(!ffmpegVideoSeek(&video, &videoFrame,time)) return false;
        if(audioInMedia) {
            ffmpegAudioSeek(&audio, time);
            soundEngineSetTime(time);
        }
        if(!playing){ //redraw
            if(ffmpegVideoGetFrame(&video,&videoFrame)){
                for(int i = 0; i < videoFrame.height; i++){
                    memcpy(
                        videoMapped + videoVulkanStride*i,
                        videoFrame.data + videoFrame.width*sizeof(uint32_t)*i,
                        videoFrame.width *sizeof(uint32_t)
                    );
                }
            }
        }
    }

    if(!fullscreen){
        float backgroundSpeed = cosf(sinf(time/20))*time;
    
        //Draw Black stuff behind 
        drawSprite((SpriteDrawCommand){
            .position = (vec2){previewPos.x,previewPos.y},
            .scale = (vec2){previewPos.width, previewPos.height},
        });
    
        float percent = videoFrame.frameTime / video.duration;
    
        float cursorWidth = timelineRect.width / 500;
        
        drawSprite((SpriteDrawCommand){
            .position = (vec2){floor(timelineRect.x+percent*timelineRect.width+cursorWidth/2),timelineRect.y},
            .scale = (vec2){cursorWidth, timelineRect.height},
            .albedo = (vec3){1.0,0.0,0.0},
        });

        float textFont = 16;

        drawSprite((SpriteDrawCommand){
            .position = (vec2){timelineRect.x, timelineRect.y+timelineRect.height - textFont * 1.5},
            .scale = (vec2){timelineRect.width, textFont * 1.5},
            .albedo = (vec3){(float)0x10 / 255,(float)0x10 / 255,(float)0x10 / 255},
        });
    
        char text[256];
    
        sprintf(text, "%.2fs/%.2fs", videoFrame.frameTime, video.duration);
    
        drawText(text, 0xFFFFFF, textFont, (Rect){
            .x = swapchainExtent.width / 2 - measureText(text,textFont) / 2,
            .y = timelineRect.y+timelineRect.height - textFont*1.5,
        });

        sprintf(text, "Loudness: %d%%", (int)floorf(soundVolume*100));

        drawText(text, 0xFFFFFF, textFont, (Rect){
            .x = 10,
            .y = timelineRect.y+timelineRect.height - textFont*1.5,
        });
    }

    return true;
}

bool draw(){
    //sprite pass
    vkCmdBeginRenderingEX(cmd, (BeginRenderingEX){
        .colorAttachment = getSwapchainImageView(),
        .clearColor = (Color){18.0f/255.f,18.0f/255.f,18.0f/255.f,1.0f},
    });
    vkCmdSetViewport(cmd, 0, 1, &(VkViewport){
        .width = swapchainExtent.width,
        .height = swapchainExtent.height
    });
        
    vkCmdSetScissor(cmd, 0, 1, &(VkRect2D){
        .extent = swapchainExtent,
    });
        
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&bindlessDescriptorSet,0,NULL);

    vkCmdPushConstants(cmd,pipelineLayout,VK_SHADER_STAGE_ALL,0,sizeof(PushConstants), &pcs);

    renderSprites();

    vkCmdEndRendering(cmd);

    //preview pass
    vkCmdBeginRenderingEX(cmd, (BeginRenderingEX){
        .colorAttachment = getSwapchainImageView(),
    });

    vkCmdSetViewport(cmd, 0, 1, &(VkViewport){
        .width = swapchainExtent.width,
        .height = swapchainExtent.height
    });
        
    vkCmdSetScissor(cmd, 0, 1, &(VkRect2D){
        .extent = swapchainExtent,
    });

    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinePreview);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayoutPreview,0,1,&descriptorSet,0,NULL);
    vkCmdPushConstants(cmd,pipelineLayoutPreview,VK_SHADER_STAGE_ALL,0,sizeof(PushConstantsPreview), &pcsPreview);

    vkCmdDraw(cmd, 6, 1, 0, 0);

    vkCmdEndRendering(cmd);
    return true;
}