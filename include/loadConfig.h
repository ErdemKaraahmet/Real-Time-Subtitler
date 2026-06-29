#pragma once
#include <SDL3/SDL.h>

typedef enum {
    CONFIG_LOAD_FILE_NOT_FOUND = -2,
    CONFIG_LOAD_NONE           = -1,
    CONFIG_LOAD_FULL           =  0,
    CONFIG_LOAD_PARTIAL        =  1
} ConfigLoadStatus;

typedef struct {
    char font[512];
    int font_size;
    int outline_thickness;
    SDL_Color text_color;
    SDL_Color text_outline_color;
    char modelPath[512];
} AppConfig;

AppConfig loadDefaultConfig();
ConfigLoadStatus loadConfig(AppConfig* conf);
bool saveConfig(const AppConfig* conf);