#version 450

#include "sprite_pcs.glsl"

layout (set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 uv;
layout(location = 1) in flat uint InstanceIndex;

void main() {
    uint textureID = (pcs.spriteDrawBuffer.commands[InstanceIndex].textureIDEffects & 0xFFFF) - 1;
    uint effects = (pcs.spriteDrawBuffer.commands[InstanceIndex].textureIDEffects >> 2*8) & 0xFFFF;

    vec2 new_uv = (uv * pcs.spriteDrawBuffer.commands[InstanceIndex].size) + pcs.spriteDrawBuffer.commands[InstanceIndex].offset;

    if(textureID == -1){
        outColor = vec4(pcs.spriteDrawBuffer.commands[InstanceIndex].albedo,1.0f);
    }else{
        if(effects == 1) // SDF
        {
            float d = texture(textures[textureID], new_uv).r;
            float aaf = fwidth(d);
            float alpha = smoothstep(0.5 - aaf, 0.5 + aaf, d);
            outColor = vec4(pcs.spriteDrawBuffer.commands[InstanceIndex].albedo, alpha);
        }else{
            outColor = texture(textures[textureID], new_uv);
        }
    }

}