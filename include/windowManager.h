#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "configManager.h"
#include "whisperEngine.h"

bool initWindow(int width, int height);
void computeContainerDimensions(TTF_Font *font, const AppConfig *config, int *outW, int *outH);
void destroyWindow(void);
void setWindowCenter(int centerX, int centerY);
void getWindowCenter(int *centerX, int *centerY);
void setWindowMoveMode(bool enabled);
bool isWindowID(SDL_WindowID id);

bool updateSubtitleText(TTF_Font *font, SubtitleToken *outputTokens, int tokenNum, const AppConfig *config, bool is_new_tokens);
void clearSubtitleText(void);
bool hasSubtitleText(void);
void renderSubtitleWindow(void);
bool updateWindowSnap(void);
void handleWindowMovedEvent(void);
