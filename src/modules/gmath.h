#ifndef MODULES_GMATH
#define MODULES_GMATH

#define PI 3.14159265358979323846

typedef struct {
    float x;
    float y;
} vec2;

typedef struct {
    float x;
    float y;
    float z;
} vec3;

typedef struct {
    float x;
    float y;
    float z;
    float w;
} vec4;

typedef struct {
    float v[16];
} mat4;

mat4 ortho2D(float width, float height);
mat4 perspective(float fov, float aspect, float zNear, float zFar);
mat4 mat4mul(mat4 *a, mat4 *b);
mat4 mat4transpose(mat4 *a);

#endif
