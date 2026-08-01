#pragma once

#include "controlPanel.h"
#include <cimgui.h>
#include <cimgui_impl.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "textTexture.h"
#include "modelManager.h"
#include "utils.h"
#include "version.h"
#include "appEvents.h"
#include "whisper.h"

#ifndef IM_COL32
#define IM_COL32(R, G, B, A) (((ImU32)(A) << 24) | ((ImU32)(B) << 16) | ((ImU32)(G) << 8) | ((ImU32)(R) << 0))
#endif

#define MAX_ITEMS 64

extern const float UI_WINDOW_WIDTH;
extern const float UI_PADDING;
extern const float UI_SPACING;
extern const float UI_BUTTON_WIDTH;
extern const float UI_PREVIEW_BOX_WIDTH;
extern const float UI_PREVIEW_BOX_HEIGHT;
extern const float UI_WINDOW_HEIGHT;

extern int g_DeleteTargetIndex;

extern SDL_Window *cpWindow;
extern SDL_Renderer *cpRenderer;
extern bool cpOpen;

extern char scannedFonts[MAX_ITEMS][256];
extern int scannedFontCount;

extern AppConfig *pLiveConfig;
extern AppConfig uiConfig;
extern AppConfig savedConfig;
extern bool modelChanged;
extern char whisperStatusMessage[256];
extern bool whisperStatusError;
extern int cpActivePage;

extern char globalUiErrorMessage[512];
extern bool showGlobalUiErrorPopup;

extern SDL_Texture *previewTexture;
extern float previewWidth;
extern float previewHeight;
extern bool previewNeedsUpdate;
extern bool previewFontLoadFailed;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
void triggerGlobalError(const char *fmt, ...);

const char *getFilenameFromPath(const char *path);
void updatePreviewTexture(void);

void renderViewPage(void);
void renderTranscriptionPage(const char *activeModelFilename, bool *triggerDeletePopup);
void renderSystemPage(void);
void renderFooter(ControlPanelStatus *status, bool isDirty);
void renderModals(const char *activeModelFilename);
