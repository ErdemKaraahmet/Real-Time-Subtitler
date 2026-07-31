#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "configManager.h"
#include "whisperEngine.h"

SDL_Texture *createTextTexture(SDL_Renderer *renderer, TTF_Font *font, SubtitleToken *textToken, int textTokenNum, const AppConfig *config,
                               float *text_width, float *text_height);
