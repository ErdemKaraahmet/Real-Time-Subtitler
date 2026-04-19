#pragma once
#include <SDL3/SDL.h>

typedef struct DragState {
    bool isDragging;
    bool isInWindow;
    bool shiftHeld;
    float dragOffsetX;
    float dragOffsetY;
} DragState;

extern DragState DragState_default;

bool createWindow(SDL_Window** window, SDL_Renderer** renderer, int width, int height);

void dragWindow(SDL_Window* window, DragState* dragState);