#include <SDL3/SDL.h>
#include "windowManager.h"

DragState DragState_default = {false, false, false, 0, 0};

bool createWindow(SDL_Window** window, SDL_Renderer** renderer, int width, int height){
    // Create a transparent window
    *window = SDL_CreateWindow("Subtitle Overlay", width, height, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP);

    if (*window == NULL) {
        // In the case that the window could not be made...
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetWindowMousePassthrough(*window, true);
    SDL_SetWindowFocusable(*window, true);
    *renderer = SDL_CreateRenderer(*window, NULL);
    
    return true;
}

void dragWindow(SDL_Window* window, DragState* drag){
    if(drag->isDragging){
        float mouseX, mouseY;
        SDL_GetGlobalMouseState(&mouseX, &mouseY);
        SDL_SetWindowPosition(window, (int)(mouseX - drag->dragOffsetX), (int)(mouseY - drag->dragOffsetY));
    }
}