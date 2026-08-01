#include <SDL3/SDL.h>
#include "windowManager.h"
#include "textTexture.h"

static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_texture = NULL;
static float s_textWidth = 0.0f;
static float s_textHeight = 0.0f;
static int s_width = 240;
static int s_height = 80;

bool initWindow(int width, int height) {
    s_width = width;
    s_height = height;

    // Create a transparent window
    s_window = SDL_CreateWindow("Subtitle Overlay", width, height,
                                SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_UTILITY);

    if (s_window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetWindowMousePassthrough(s_window, true);
    SDL_SetWindowFocusable(s_window, true);
    s_renderer = SDL_CreateRenderer(s_window, NULL);
    if (s_renderer == NULL) {
        SDL_DestroyWindow(s_window);
        s_window = NULL;
        return false;
    }

    return true;
}

void clearSubtitleText(void) {
    if (s_texture != NULL) {
        SDL_DestroyTexture(s_texture);
        s_texture = NULL;
    }
    s_textWidth = 0.0f;
    s_textHeight = 0.0f;
}

bool hasSubtitleText(void) {
    return s_texture != NULL;
}

void destroyWindow(void) {
    clearSubtitleText();
    if (s_renderer) {
        SDL_DestroyRenderer(s_renderer);
        s_renderer = NULL;
    }
    if (s_window) {
        SDL_DestroyWindow(s_window);
        s_window = NULL;
    }
}

void setWindowCenter(int centerX, int centerY) {
    if (s_window && centerX != -1 && centerY != -1) {
        int initX = centerX - (s_width / 2);
        int initY = centerY - (s_height / 2);
        SDL_SetWindowPosition(s_window, initX, initY);
    }
}

void getWindowCenter(int *centerX, int *centerY) {
    if (!s_window)
        return;
    int winX, winY, winW, winH;
    SDL_GetWindowPosition(s_window, &winX, &winY);
    SDL_GetWindowSize(s_window, &winW, &winH);
    if (centerX)
        *centerX = winX + (winW / 2);
    if (centerY)
        *centerY = winY + (winH / 2);
}

bool updateSubtitleText(TTF_Font *font, SubtitleToken *outputTokens, int tokenNum, const AppConfig *config) {
    clearSubtitleText();
    if (!s_renderer || !font || !outputTokens || tokenNum <= 0 || !config) {
        return false;
    }

    s_texture = createTextTexture(s_renderer, font, outputTokens, tokenNum, config, &s_textWidth, &s_textHeight);
    if (s_texture != NULL) {
        SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);

        int currentX, currentY;
        SDL_GetWindowPosition(s_window, &currentX, &currentY);
        int offsetX = (int)(((float)s_width - s_textWidth) * 0.5f);
        int offsetY = (int)(((float)s_height - s_textHeight) * 0.5f);
        SDL_SetWindowPosition(s_window, currentX + offsetX, currentY + offsetY);

        s_width = (int)s_textWidth;
        s_height = (int)s_textHeight;
        SDL_SetWindowSize(s_window, s_width, s_height);
        return true;
    }
    return false;
}

void renderSubtitleWindow(void) {
    if (!s_renderer)
        return;
    SDL_RenderClear(s_renderer);
    if (s_texture != NULL) {
        SDL_FRect dstRect = {0.0f, 0.0f, s_textWidth, s_textHeight};
        SDL_RenderTexture(s_renderer, s_texture, NULL, &dstRect);
    }
    SDL_RenderPresent(s_renderer);
}

void setWindowMoveMode(bool enabled) {
    if (!s_window)
        return;
    SDL_SetWindowMousePassthrough(s_window, !enabled);
    SDL_SetWindowBordered(s_window, enabled);
}

bool isWindowID(SDL_WindowID id) {
    return s_window && (SDL_GetWindowID(s_window) == id);
}
