#pragma once
#include <SDL3/SDL.h>

typedef struct {
    int font_size;
    int outline_thickness;
    SDL_Color text_color;
    SDL_Color text_outline_color;
    char modelPath[512];
} AppConfig;

AppConfig loadDefaultConfig();
bool loadConfig(AppConfig* conf);
bool saveConfig(const AppConfig* conf);