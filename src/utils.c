#include <SDL3/SDL.h>
#include <stdio.h>
#include "utils.h"

void utilsResolvePath(char* dest, size_t destSize, const char* relativePath) {
    const char* basePath = SDL_GetBasePath();
    if (basePath) {
        snprintf(dest, destSize, "%s%s", basePath, relativePath);
    } else {
        SDL_strlcpy(dest, relativePath, destSize);
    }
}
