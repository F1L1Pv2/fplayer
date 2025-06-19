#ifndef FVFX_ENGINE
#define FVFX_ENGINE

#include <stdbool.h>
#include <stddef.h>

bool engineInit(char* title, size_t width, size_t height);
int engineStart();

#endif