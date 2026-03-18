#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "loadConfig.h"

SDL_Texture* createTextTexture(SDL_Renderer* renderer, TTF_Font* font, const char* text, AppConfig config, float* text_width, float* text_height){
    SDL_Color bgColor = config.text_outline_color;
    SDL_Color fgColor = config.text_color;

    int thickness = config.outline_thickness;
    TTF_SetFontOutline(font, thickness); // set thickness
    SDL_Surface* backGroundText = TTF_RenderText_Blended(font, text, 0, bgColor);
    
    TTF_SetFontOutline(font, 0); // set thickness
    SDL_Surface* foreGroundText = TTF_RenderText_Blended(font, text, 0, fgColor);
    
    SDL_Rect destinationRect = {thickness, thickness, backGroundText->w, backGroundText->h};
    SDL_BlitSurface(foreGroundText, NULL, backGroundText, &destinationRect); // combine surfaces into backGrounText
    SDL_Surface* surface = backGroundText; 
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    *text_width = (float)surface->w;
    *text_height = (float)surface->h;

    SDL_DestroySurface(foreGroundText);
    SDL_DestroySurface(surface); // Clean up surface 

    return texture;
}