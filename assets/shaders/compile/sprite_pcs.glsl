#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_nonuniform_qualifier: require

struct SpriteDrawCommand {
    vec2 position;
    vec2 scale;
    uint textureIDEffects;  // 0xAAAABBBB lowest bits are textures and higher are effects
    vec3 albedo;
    vec2 offset;
    vec2 size;
};

layout (buffer_reference, scalar) readonly buffer SpriteDrawBuffer {
    SpriteDrawCommand commands[];
};

layout (push_constant) uniform constants
{
    mat4 projView;
    SpriteDrawBuffer spriteDrawBuffer;
} pcs;