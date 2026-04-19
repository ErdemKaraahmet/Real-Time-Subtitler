#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "loadConfig.h"
#include "windowManager.h"
#include "textTexture.h"
#include "audioCapture.h"
#include "whisperEngine.h"
#include "trayManager.h"

#define SAMPLE_RATE 16000             // 16Khz
#define SAMPLE_SIZE (SAMPLE_RATE * 2) // 2 second * sample rate = 32000 frames 

// shared state between threads
static char subtitleText[124] = "";
static float audioChunk[SAMPLE_SIZE];
static bool chunkReady = false;
static bool textUpdated = false;
static bool paused = false;
static SDL_Mutex *textMutex;
static SDL_Texture *texture = NULL; // promoted so pause handler can clear it

int whisperThread(void *data);

void handleEvents(SDL_Window *window, bool *done, DragState *dragState);

int main(int argc, char *argv[])
{
    // Initialize SDL and TTF
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    // Load user config
    AppConfig config_obj = loadDefaultConfig();
    AppConfig *config = &config_obj;
    if(loadConfig(config)){
        SDL_Log("Configuration is loaded");
    }
    else {
        SDL_Log("Default configuration is loaded");
    }

    initAndStartAudio();
    whisperInit(config->modelPath);

    // Create a transparent window
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width = 240, height = 80;
    if (!createWindow(&window, &renderer, width, height))
    {
        SDL_Log("Couldn't load font: %s", SDL_GetError());
        return 1;
    }

    initTray(window);

    // Load a font
    TTF_Font *font = TTF_OpenFont(config->font, config->font_size); // Path to your font and size
    if (!font)
    {
        SDL_Log("Couldn't load font: %s", SDL_GetError());
        return 1;
    }

    // Create the text surface and texture
    float text_width, text_height;

    DragState dragState = DragState_default;

    textMutex = SDL_CreateMutex();
    SDL_CreateThread(whisperThread, "whisper", NULL);

    bool done = false;
    while (!done)
    {
        
        if (audioChunkReady(SAMPLE_SIZE) && !chunkReady)
        {
            getAudioChunk(audioChunk, SAMPLE_SIZE);
            chunkReady = true; // signal the whisper thread
        }

        if (textUpdated)
        {
            SDL_LockMutex(textMutex);
            if (texture != NULL) SDL_DestroyTexture(texture);
            if (!strcmp(subtitleText, " [BLANK_AUDIO]")) subtitleText[0] = '\0'; // whisper outputs " [BLANK_AUDIO]" on empty audio, to not print it exactly
            texture = createTextTexture(renderer, font, subtitleText, config, &text_width, &text_height);

            // Resize the window to fit snugly to the text
            if (texture != NULL) {
                
                int currentX, currentY;
                SDL_GetWindowPosition(window, &currentX, &currentY);
                SDL_SetWindowPosition(window, (int)(currentX + (width - text_width)/2), (int)(currentY + (height - text_height)/2));

                width = (int)text_width; // update
                height = (int)text_height; 

                SDL_SetWindowSize(window, width, height);
            }
            
            textUpdated = false;
            SDL_UnlockMutex(textMutex);
        }
        handleEvents(window, &done, &dragState);

        // If we are dragging move the window
        dragWindow(window, &dragState);

        SDL_RenderClear(renderer);

        // Draw the text in the center
        if (texture != NULL)
        {
            SDL_FRect dstRect = {(width - text_width) / 2, (height - text_height) / 2, text_width, text_height};
            SDL_RenderTexture(renderer, texture, NULL, &dstRect);
        }
        SDL_RenderPresent(renderer);

        int fps = dragState.isDragging ? 60 : 30;
        SDL_Delay(1000 / fps);
    }

    // Close and destroy the window
    SDL_DestroyWindow(window);

    // Clean up
    whisperFree();
    cleanupAudio();
    if (texture) SDL_DestroyTexture(texture);
    destroyTray();
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    return 0;
}

void handleEvents(SDL_Window *window, bool *done, DragState *drag)
{

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            *done = true;
        }

        if (event.type == SDL_EVENT_USER)
        {
            if (event.user.code == 1) {
                paused = true;
                pauseAudio();
                // Immediately clear the on-screen text
                subtitleText[0] = '\0';
                if (texture) { SDL_DestroyTexture(texture); texture = NULL; }
            }
            else {
                paused = false;
                resumeAudio();
            }
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
        {
            SDL_SetWindowMousePassthrough(window, true);
            SDL_SetWindowBordered(window, false);
        }
    }
}

int whisperThread(void *data)
{
    while (true)
    {
        if (chunkReady && !paused)
        {
            SDL_LockMutex(textMutex);
            subtitleText[0] = '\0';
            whisperProcess(audioChunk, SAMPLE_SIZE, subtitleText, sizeof(subtitleText));
            textUpdated = true;
            SDL_UnlockMutex(textMutex);
            chunkReady = false;
        }
        else if (chunkReady && paused)
        {
            chunkReady = false; // discard stale audio collected while paused
        }
        SDL_Delay(10);
    }
    return 0;
}