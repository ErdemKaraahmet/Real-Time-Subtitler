#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>
#include "windowManager.h"
#include "textTexture.h"

#define SNAP_THRESHOLD 30
#define SNAP_DRAG_END_DEBOUNCE_MS 120
#define SNAP_ANIM_MS 140

static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_texture = NULL;
static float s_textWidth = 0.0f;
static float s_textHeight = 0.0f;
static int s_width = 800;
static int s_height = 100;

// Magnetic Window Snapping State
typedef struct {
    bool dragActive;     // true while WINDOW_MOVED events are still arriving
    Uint64 lastMoveTime; // timestamp of the most recent WINDOW_MOVED event
    bool animating;      // true while tweening into the snapped position
    Uint64 animStart;
    int fromX, fromY;
    int toX, toY;
    bool isProgrammaticMove; // true if position update was triggered by code, not user drag
} SnapState;

static SnapState s_snapState = {0};

void computeContainerDimensions(TTF_Font *font, const AppConfig *config, int *outW, int *outH) {
    int newW = 1000;
    int newH = s_height;

    if (font && config) {
        int fontH = TTF_GetFontHeight(font);
        int thickness = config->outline_thickness;
        int lineH = fontH + (2 * thickness) + 4;
        newH = 2 * lineH;
    }

    if (s_window != NULL && (s_width != newW || s_height != newH)) {
        int curX = 0, curY = 0, curW = 0, curH = 0;
        if (SDL_GetWindowPosition(s_window, &curX, &curY) && SDL_GetWindowSize(s_window, &curW, &curH)) {
            int centerX = curX + (curW / 2);
            int centerY = curY + (curH / 2);
            int targetX = centerX - (newW / 2);
            int targetY = centerY - (newH / 2);

            s_snapState.isProgrammaticMove = true;
            SDL_SetWindowPosition(s_window, targetX, targetY);
        }
        s_snapState.isProgrammaticMove = true;
        SDL_SetWindowSize(s_window, newW, newH);
    }

    s_width = newW;
    s_height = newH;

    if (outW)
        *outW = s_width;
    if (outH)
        *outH = s_height;
}

bool initWindow(int width, int height) {
    if (width > 0)
        s_width = width;
    if (height > 0)
        s_height = height;

    // Create a transparent window
    s_window = SDL_CreateWindow("Subtitle Overlay", s_width, s_height,
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

bool updateSubtitleText(TTF_Font *font, SubtitleToken *outputTokens, int tokenNum, const AppConfig *config, bool is_new_tokens) {
    clearSubtitleText();
    if (!s_renderer || !font || !outputTokens || tokenNum <= 0 || !config) {
        return false;
    }

    s_texture = createTextTexture(s_renderer, font, outputTokens, tokenNum, config, &s_textWidth, &s_textHeight, is_new_tokens);
    if (s_texture != NULL) {
        SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);

        if (!s_snapState.dragActive) {
            int currentX = 0, currentY = 0;
            if (SDL_GetWindowPosition(s_window, &currentX, &currentY)) {
                int offsetX = (int)(((float)s_width - s_textWidth) * 0.5f);
                int offsetY = (int)(((float)s_height - s_textHeight) * 0.5f);
                if (offsetX != 0 || offsetY != 0) {
                    s_snapState.isProgrammaticMove = true;
                    SDL_SetWindowPosition(s_window, currentX + offsetX, currentY + offsetY);
                }
            }
            if (s_width != (int)s_textWidth || s_height != (int)s_textHeight) {
                s_width = (int)s_textWidth;
                s_height = (int)s_textHeight;
                s_snapState.isProgrammaticMove = true;
                SDL_SetWindowSize(s_window, s_width, s_height);
            }
        }
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

// Magnetic Window Snapping

static void beginSnapCheck(void) {
    if (!s_window)
        return;
    SDL_DisplayID displayID = SDL_GetDisplayForWindow(s_window);
    if (!displayID)
        return;
    SDL_Rect displayBounds = {0};
    if (!SDL_GetDisplayUsableBounds(displayID, &displayBounds))
        return;

    int winX = 0, winY = 0, winW = 0, winH = 0;
    if (!SDL_GetWindowPosition(s_window, &winX, &winY))
        return;
    if (!SDL_GetWindowSize(s_window, &winW, &winH))
        return;

    if (winW <= 0 || winH <= 0 || displayBounds.w <= 0 || displayBounds.h <= 0)
        return;

    int displayCenterX = displayBounds.x + (displayBounds.w / 2);
    int displayCenterY = displayBounds.y + (displayBounds.h / 2);
    int winCenterX = winX + (winW / 2);
    int winCenterY = winY + (winH / 2);

    int targetX = winX;
    int targetY = winY;
    bool shouldSnap = false;

    if (abs(winCenterX - displayCenterX) <= SNAP_THRESHOLD) {
        targetX = displayCenterX - (winW / 2);
        shouldSnap = true;
    }
    if (abs(winCenterY - displayCenterY) <= SNAP_THRESHOLD) {
        targetY = displayCenterY - (winH / 2);
        shouldSnap = true;
    }

    if (shouldSnap && (targetX != winX || targetY != winY)) {
        s_snapState.fromX = winX;
        s_snapState.fromY = winY;
        s_snapState.toX = targetX;
        s_snapState.toY = targetY;
        s_snapState.animating = true;
        s_snapState.animStart = SDL_GetTicks();
    }
}

bool updateWindowSnap(void) {
    bool busy = false;
    if (s_snapState.dragActive) {
        Uint64 now = SDL_GetTicks();
        if (now - s_snapState.lastMoveTime > SNAP_DRAG_END_DEBOUNCE_MS) {
            if ((SDL_GetGlobalMouseState(NULL, NULL) & SDL_BUTTON_LMASK) == 0) {
                s_snapState.dragActive = false;
                beginSnapCheck();
            } else {
                busy = true;
            }
        } else {
            busy = true;
        }
    }

    if (s_snapState.animating) {
        Uint64 elapsed = SDL_GetTicks() - s_snapState.animStart;
        float t = (float)elapsed / (float)SNAP_ANIM_MS;
        if (t >= 1.0f) {
            s_snapState.isProgrammaticMove = true;
            SDL_SetWindowPosition(s_window, s_snapState.toX, s_snapState.toY);
            s_snapState.animating = false;
        } else {
            float eased = 1.0f - powf(1.0f - t, 3.0f); // ease-out cubic
            int x = s_snapState.fromX + (int)((float)(s_snapState.toX - s_snapState.fromX) * eased);
            int y = s_snapState.fromY + (int)((float)(s_snapState.toY - s_snapState.fromY) * eased);
            s_snapState.isProgrammaticMove = true;
            SDL_SetWindowPosition(s_window, x, y);
            busy = true;
        }
    }
    return busy;
}

void handleWindowMovedEvent(void) {
    if (s_snapState.isProgrammaticMove) {
        s_snapState.isProgrammaticMove = false;
    } else {
        s_snapState.dragActive = true;
        s_snapState.lastMoveTime = SDL_GetTicks();
        s_snapState.animating = false;
    }
}
