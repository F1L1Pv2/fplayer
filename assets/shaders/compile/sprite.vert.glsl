#version 450

#include "sprite_pcs.glsl"

layout(location = 0) out vec2 uv;
layout(location = 1) out flat uint InstanceIndex;

void main() {
    uint b = 1 << (gl_VertexIndex % 6);
    vec2 baseCoord = vec2((0x1C & b) != 0, (0xE & b) != 0);
    uv = baseCoord;

    vec2 pos = pcs.spriteDrawBuffer.commands[gl_InstanceIndex].position;
    vec2 scale = pcs.spriteDrawBuffer.commands[gl_InstanceIndex].scale;

    mat4 model = mat4(
        vec4(scale.x,0,0,0),
        vec4(0,scale.y,0,0),
        vec4(0,0,1,0),
        vec4(pos.x,pos.y,0,1)
    );

    gl_Position = pcs.projView * model * vec4(baseCoord, 0.0f, 1.0f);
    InstanceIndex = gl_InstanceIndex;
}