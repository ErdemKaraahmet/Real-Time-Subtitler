#pragma once
#include <SDL3/SDL.h>
#include "loadConfig.h"

typedef struct {
    bool configSaved;
    bool modelChanged;
} ControlPanelStatus;

void openControlPanel(AppConfig* liveConfig);
void openControlPanelToTranscriptionWithError(AppConfig* liveConfig, const char* errorMessage);
void handleControlPanelEvent(const SDL_Event* event);
ControlPanelStatus updateAndRenderControlPanel(SDL_Renderer* overlayRenderer);
void closeControlPanel(void);
bool isControlPanelOpen(void);
void setControlPanelWhisperError(bool error, const char* message);
