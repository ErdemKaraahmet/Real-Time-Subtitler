#include <SDL3/SDL.h>

typedef struct {
    int font_size;
    int outline_thickness;
    SDL_Color text_color;
    SDL_Color text_outline_color;
} AppConfig;

AppConfig loadConfig();