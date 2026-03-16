// Source - https://stackoverflow.com/a/51956224

// SDL window with transparent background
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <Windows.h>
#include <stdbool.h>

// Makes a window transparent by setting a transparency color.
bool MakeWindowTransparent(SDL_Window* window, COLORREF colorKey) {
    HWND hWnd = (HWND)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        NULL
    );

    if (!hWnd) return false;

    SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    return SetLayeredWindowAttributes(hWnd, colorKey, 0, LWA_COLORKEY);
}

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);

    // Get resolution of primary monitor
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
    int desktopWidth  = mode->w;
    int desktopHeight = mode->h;

    SDL_Window* window = SDL_CreateWindow(
        "SDL Transparent Window",
        desktopWidth/3, desktopHeight/3,
        SDL_WINDOW_BORDERLESS
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

    // Set background color to magenta and clear screen
    //SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
    SDL_RenderClear(renderer);

    // Draw blue square in top-left corner
    SDL_FRect rect1 = {0, 0, 100, 100};
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderFillRect(renderer, &rect1);

    // Draw red square in center of the screen
    SDL_FRect rect2 = {(desktopWidth-100)/2.0f, (desktopHeight-100)/2.0f, 100, 100};
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderFillRect(renderer, &rect2);

    // Add window transparency (Magenta will be see-through)
    MakeWindowTransparent(window, RGB(255, 0, 255));

    // Render to screen
    SDL_RenderPresent(renderer);

    // Loop until user quits
    bool quit = false;
    SDL_Event event;
    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            }
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}