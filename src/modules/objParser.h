#ifndef MODULES_OBJ_PARSER
#define MODULES_OBJ_PARSER

#include "modules/gmath.h"

typedef struct{
    vec3 position;
    vec3 normal;
} Vertex;

typedef struct{
    Vertex* items;
    size_t count;
    size_t capacity;
} Vertices;

void wavefrontParse(char* data, Vertices* verticesOut);

#endif