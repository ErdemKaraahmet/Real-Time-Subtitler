#include <SDL3/SDL.h>
#include <stdio.h>
#include "utils.h"

bool utilsResolvePath(char *dest, size_t destSize, const char *relativePath) {
    const char *basePath = SDL_GetBasePath();
    if (basePath) {
        int res = snprintf(dest, destSize, "%s%s", basePath, relativePath);
        if (res >= 0 && (size_t)res < destSize) {
            return true;
        }
    }
    SDL_strlcpy(dest, relativePath, destSize);
    return false;
}

bool utilsIsFileReadable(const char *relativePath) {
    char fullPath[512];
    utilsResolvePath(fullPath, sizeof(fullPath), relativePath);
    return SDL_GetPathInfo(fullPath, NULL);
}
