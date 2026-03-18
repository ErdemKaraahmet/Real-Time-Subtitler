#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "loadConfig.h"

SDL_Texture* createTextTexture(SDL_Renderer* renderer, TTF_Font* font, const char* text, AppConfig config, float* text_width, float* text_height);