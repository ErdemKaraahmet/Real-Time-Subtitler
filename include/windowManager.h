#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "configManager.h"
#include "whisperEngine.h"

bool initWindow(int width, int height);
void destroyWindow(void);
void setWindowCenter(int centerX, int centerY);
void getWindowCenter(int *centerX, int *centerY);
void setWindowMoveMode(bool enabled);
bool isWindowID(SDL_WindowID id);

bool updateSubtitleText(TTF_Font *font, SubtitleToken *outputTokens, int tokenNum, const AppConfig *config);
void clearSubtitleText(void);
bool hasSubtitleText(void);
void renderSubtitleWindow(void);
