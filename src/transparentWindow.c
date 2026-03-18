#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

bool createWindow(SDL_Window** window, SDL_Renderer** renderer, int width, int height);

int main(int argc, char* argv[]) {
    // Initialize  TTF
    TTF_Init();

    // Create a transparent window
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width = 640, height = 480;
    createWindow(&window, &renderer, width, height);
    

    // Load a font
    TTF_Font* font = TTF_OpenFont("fonts/cascadia.mono.ttf", 36); // Path to your font and size
    if (!font) {
        SDL_Log("Couldn't load font: %s", SDL_GetError());
        return 1;
    }

    // Create the text surface and texture
    char *text = "Hello World"; // Read-only text

    SDL_Color bgColor = {0,0,0,0};
    SDL_Color fgColor = {255, 255, 255, 255};

    int thickness = 4;
    TTF_SetFontOutline(font, thickness); // set thickness
    SDL_Surface* backGroundText = TTF_RenderText_Blended(font, text, 0, bgColor);
    
    TTF_SetFontOutline(font, 0); // set thickness
    SDL_Surface* foreGroundText = TTF_RenderText_Blended(font, text, 0, fgColor);
    
    SDL_Rect destinationRect = {thickness, thickness, backGroundText->w, backGroundText->h};
    SDL_BlitSurface(foreGroundText, NULL, backGroundText, &destinationRect); // combine surfaces into backGrounText
    SDL_Surface* surface = backGroundText; 
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    float text_width = (float)surface->w;
    float text_height = (float)surface->h;
    SDL_DestroySurface(surface); // Clean up surface 

    // Main Loop
    bool isInWindow = false;
    bool isDragging = false;
    float dragOffsetX = 0, dragOffsetY = 0;
    bool shiftHeld = false;

    bool done = false;
    SDL_Event event;
    while (!done) {
        
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                done = true;
            }
            // Is shift key
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_LSHIFT || event.key.key == SDLK_RSHIFT) {
                    shiftHeld = true;
                    SDL_SetWindowMousePassthrough(window, false);
                    //SDL_Log("passthrough off");
                }
            }
            if (event.type == SDL_EVENT_KEY_UP) {
                if (event.key.key == SDLK_LSHIFT || event.key.key == SDLK_RSHIFT) {
                    shiftHeld = false;
                    isDragging = false;
                    SDL_SetWindowMousePassthrough(window, true);
                    //SDL_Log("passthrough on");
                }
            }
            if (event.type == SDL_EVENT_WINDOW_MOUSE_ENTER) isInWindow = true;
            if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) isInWindow = false;
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN){
                if ((SDL_GetModState() & SDL_KMOD_SHIFT) && isInWindow) {
                    isDragging = true;

                    // Save offset between mouse and window origin at drag start
                    float mouseX, mouseY;
                    int winX, winY;
                    SDL_GetGlobalMouseState(&mouseX, &mouseY);
                    SDL_GetWindowPosition(window, &winX, &winY);
                    dragOffsetX = mouseX - winX;
                    dragOffsetY = mouseY - winY;
                }
            } 
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) isDragging = false;
        }

        if ((SDL_GetModState() & SDL_KMOD_SHIFT)) {
            SDL_SetWindowMousePassthrough(window, false);            
        }
        else {
            SDL_SetWindowMousePassthrough(window, true);
        }
        

        // If we are dragging move the window
        if(isDragging){
            float mouseX, mouseY;
            SDL_GetGlobalMouseState(&mouseX, &mouseY);
            SDL_SetWindowPosition(window, (int)(mouseX - dragOffsetX), (int)(mouseY - dragOffsetY));
        }

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

bool createWindow(SDL_Window** window, SDL_Renderer** renderer, int width, int height){
    // Initialize SDL
    SDL_Init(SDL_INIT_VIDEO);
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