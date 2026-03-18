#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "loadConfig.h"
#include "windowManager.h"
#include "textTexture.h"

void handleEvents(SDL_Window* window, bool* done, DragState* dragState);

int main(int argc, char* argv[]) {
    // Initialize SDL and TTF
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    // Create a transparent window
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width = 640, height = 480;
    if (!createWindow(&window, &renderer, width, height)) {
        SDL_Log("Couldn't load font: %s", SDL_GetError());
        return 1;
    }

    // Load user config
    AppConfig config = loadConfig();

    // Load a font
    TTF_Font* font = TTF_OpenFont("fonts/cascadia.mono.ttf", config.font_size); // Path to your font and size
    if (!font) {
        SDL_Log("Couldn't load font: %s", SDL_GetError());
        return 1;
    }

    char *text = "Hello World"; // Read-only text

    // Create the text surface and texture
    float text_width, text_height; 
    SDL_Texture* texture = createTextTexture(renderer, font, text, config, &text_width, &text_height);

    // Main Loop
    DragState dragState = DragState_default;

    bool done = false;
    while (!done) {
        
        handleEvents(window, &done, &dragState);
        
        // If we are dragging move the window
        dragWindow(window, &dragState);

        SDL_RenderClear(renderer);

        // Draw the text in the center
        SDL_FRect dstRect = { (width - text_width) / 2, (height - text_height) / 2, text_width, text_height };
        SDL_RenderTexture(renderer, texture, NULL, &dstRect);

        SDL_RenderPresent(renderer);
    }

    // Close and destroy the window
    SDL_DestroyWindow(window);

    // Clean up
    SDL_DestroyTexture(texture);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    return 0;
}

void handleEvents(SDL_Window* window, bool* done, DragState* drag){
    
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                *done = true;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_LSHIFT || event.key.key == SDLK_RSHIFT) {
                    drag->shiftHeld = true;
                    SDL_SetWindowMousePassthrough(window, false);
                }
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_LSHIFT || event.key.key == SDLK_RSHIFT) {
                    drag->shiftHeld = false;
                    drag->isDragging = false;
                    SDL_SetWindowMousePassthrough(window, true);
                }
            }
            if (event.type == SDL_EVENT_WINDOW_MOUSE_ENTER) drag->isInWindow = true;
            if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) drag->isInWindow = false;
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN){
                if ((SDL_GetModState() & SDL_KMOD_SHIFT) && drag->isInWindow) {
                    drag->isDragging = true;
                    float mouseX, mouseY;
                    int winX, winY;
                    SDL_GetGlobalMouseState(&mouseX, &mouseY);
                    SDL_GetWindowPosition(window, &winX, &winY);
                    drag->dragOffsetX = mouseX - winX;
                    drag->dragOffsetY = mouseY - winY;
                }
            } 
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) drag->isDragging = false;
        }

        if (SDL_GetModState() & SDL_KMOD_SHIFT) {
            SDL_SetWindowMousePassthrough(window, false);            
        } else {
            SDL_SetWindowMousePassthrough(window, true);
        }
}