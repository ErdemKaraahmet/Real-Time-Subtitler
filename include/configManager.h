#pragma once
#include <SDL3/SDL.h>
#include <stdbool.h>

typedef enum { CONFIG_LOAD_FILE_NOT_FOUND = -2, CONFIG_LOAD_PARSE_ERROR = -1, CONFIG_LOAD_OK = 0 } ConfigLoadStatus;

typedef struct {
    char font[512];
    int font_size;
    int outline_thickness;
    int display_mode; // 0 for normal, 1 for confidence opacity
    SDL_Color text_color;
    SDL_Color text_outline_color;
    char modelPath[512];
    bool use_gpu;
    int cpu_threads;
    char language[8];
    int window_x;
    int window_y;
} AppConfig;

AppConfig loadDefaultConfig(void);
ConfigLoadStatus loadConfig(AppConfig *conf);
bool saveConfig(const AppConfig *conf);
