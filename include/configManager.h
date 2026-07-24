#pragma once
#include <SDL3/SDL.h>
#include <stdbool.h>

typedef enum { CONFIG_LOAD_FILE_NOT_FOUND = -2, CONFIG_LOAD_PARSE_ERROR = -1, CONFIG_LOAD_OK = 0 } ConfigLoadStatus;

typedef struct {
    char font[512];
    int font_size;
    int outline_thickness;
    SDL_Color text_color;
    SDL_Color text_outline_color;
    char modelPath[512];
    bool use_gpu;
} AppConfig;

AppConfig loadDefaultConfig(void);
ConfigLoadStatus loadConfig(AppConfig *conf);
bool saveConfig(const AppConfig *conf);