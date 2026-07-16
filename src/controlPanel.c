#include "controlPanel.h"
#include <cimgui.h>
#include <cimgui_impl.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "textTexture.h"

#ifndef IM_COL32
#define IM_COL32(R,G,B,A) (((ImU32)(A)<<24) | ((ImU32)(B)<<16) | ((ImU32)(G)<<8) | ((ImU32)(R)<<0))
#endif

// External getter for pause state from main.c
extern bool isAppPaused(void);

// External C++ declarations from our bridge
extern bool ImGui_ImplSDLRenderer3_Init_C(SDL_Renderer* renderer);
extern void ImGui_ImplSDLRenderer3_Shutdown_C(void);
extern void ImGui_ImplSDLRenderer3_NewFrame_C(void);
extern void ImGui_ImplSDLRenderer3_RenderDrawData_C(ImDrawData* draw_data, SDL_Renderer* renderer);

// UI Styling Constants
static const float UI_WINDOW_WIDTH = 710.0f;
static const float UI_PADDING = 12.0f;
static const float UI_SPACING = 8.0f;
static const float UI_BUTTON_WIDTH = 120.0f;
static const float UI_BUTTON_HEIGHT = 30.0f;
static const float UI_PREVIEW_BOX_WIDTH = 556.0f; // 580 - 24
static const float UI_PREVIEW_BOX_HEIGHT = 100.0f;
static const float UI_WINDOW_HEIGHT = 450.0f;

// Window and Renderer state
static SDL_Window* cpWindow = NULL;
static SDL_Renderer* cpRenderer = NULL;
static bool cpOpen = false;

// Scanned items
#define MAX_ITEMS 64
static char scannedFonts[MAX_ITEMS][256];
static int scannedFontCount = 0;

static char scannedModels[MAX_ITEMS][256];
static int scannedModelCount = 0;

// UI configuration state
static AppConfig* pLiveConfig = NULL;
static AppConfig uiConfig;
static AppConfig savedConfig; // to track dirty state
static bool modelChanged = false;
static char whisperStatusMessage[256] = "Status: Active";
static bool whisperStatusError = false;
static char cpErrorMessage[256] = "";
static bool cpHasError = false;
static int cpActivePage = 0; // 0 = View, 1 = Transcription

// Preview state
static SDL_Texture* previewTexture = NULL;
static float previewWidth = 0.0f;
static float previewHeight = 0.0f;
static bool previewNeedsUpdate = true;
static bool previewFontLoadFailed = false;


// Helper to get filename from path
static const char* getFilenameFromPath(const char* path) {
    const char* lastSlash = strrchr(path, '/');
    const char* lastBackslash = strrchr(path, '\\');
    const char* filename = path;
    if (lastSlash && lastSlash > filename) filename = lastSlash + 1;
    if (lastBackslash && lastBackslash > filename) filename = lastBackslash + 1;
    return filename;
}

// Directory enumeration callbacks
static SDL_EnumerationResult SDLCALL scanFontsCallback(void *userdata, const char *dirname, const char *fname) {
    (void)userdata; (void)dirname;
    if (scannedFontCount < MAX_ITEMS) {
        size_t len = strlen(fname);
        if (len > 4 && SDL_strcasecmp(fname + len - 4, ".ttf") == 0) {
            SDL_strlcpy(scannedFonts[scannedFontCount], fname, sizeof(scannedFonts[scannedFontCount]));
            scannedFontCount++;
        }
    }
    return SDL_ENUM_CONTINUE;
}

static SDL_EnumerationResult SDLCALL scanModelsCallback(void *userdata, const char *dirname, const char *fname) {
    (void)userdata; (void)dirname;
    if (scannedModelCount < MAX_ITEMS) {
        size_t len = strlen(fname);
        if (len > 4 && SDL_strcasecmp(fname + len - 4, ".bin") == 0) {
            SDL_strlcpy(scannedModels[scannedModelCount], fname, sizeof(scannedModels[scannedModelCount]));
            scannedModelCount++;
        }
    }
    return SDL_ENUM_CONTINUE;
}


void openControlPanel(AppConfig* liveConfig) {
    if (cpOpen) {
        // Bring to front
        SDL_RaiseWindow(cpWindow);
        return;
    }

    // Copy live config to our UI working state
    pLiveConfig = liveConfig;
    uiConfig = *liveConfig;
    savedConfig = *liveConfig;
    modelChanged = false;

    // Scan directories
    scannedFontCount = 0;
    scannedModelCount = 0;

    char path[512];
    const char* basePath = SDL_GetBasePath();
    
    snprintf(path, sizeof(path), "%sfonts", basePath);
    SDL_EnumerateDirectory(path, scanFontsCallback, NULL);
    
    snprintf(path, sizeof(path), "%smodels", basePath);
    SDL_EnumerateDirectory(path, scanModelsCallback, NULL);

    // Create window & renderer
    cpWindow = SDL_CreateWindow("RTS Control Panel", (int)UI_WINDOW_WIDTH, (int)UI_WINDOW_HEIGHT, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!cpWindow) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Control Panel window: %s", SDL_GetError());
        return;
    }

    // Set window icon
    char iconPath[512];
    snprintf(iconPath, sizeof(iconPath), "%sspaceholder_rts_icon.png", basePath);
    SDL_Surface* icon = SDL_LoadPNG(iconPath);
    if (icon) {
        SDL_SetWindowIcon(cpWindow, icon);
        SDL_DestroySurface(icon);
    }

    cpRenderer = SDL_CreateRenderer(cpWindow, NULL);
    if (!cpRenderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Control Panel renderer: %s", SDL_GetError());
        SDL_DestroyWindow(cpWindow);
        cpWindow = NULL;
        return;
    }

    // Initialize ImGui
    igCreateContext(NULL);
    ImGuiIO* io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load Cascadia Font for UI
    char uiFontPath[512];
    snprintf(uiFontPath, sizeof(uiFontPath), "%sfonts/cascadia.mono.ttf", basePath);
    ImFontAtlas_AddFontFromFileTTF(io->Fonts, uiFontPath, 16.0f, NULL, NULL);

    // Setup style
    igStyleColorsDark(NULL);
    ImGuiStyle* style = igGetStyle();
    style->WindowPadding = (ImVec2_c){UI_PADDING, UI_PADDING};
    style->ItemSpacing = (ImVec2_c){UI_SPACING, UI_SPACING};
    style->FramePadding = (ImVec2_c){6.0f, 6.0f};
    style->ButtonTextAlign = (ImVec2_c){0.5f, 0.5f};

    // Flat Geometry
    style->WindowRounding = 0.0f;
    style->FrameRounding = 0.0f;
    style->GrabRounding = 0.0f;
    style->ScrollbarRounding = 0.0f;
    style->PopupRounding = 0.0f;
    style->TabRounding = 0.0f;
    style->WindowBorderSize = 1.0f;
    style->FrameBorderSize = 1.0f;
    style->PopupBorderSize = 1.0f;

    // Monochrome Palette
    ImVec4* colors = style->Colors;
    colors[ImGuiCol_WindowBg]             = (ImVec4_c){0.00f, 0.00f, 0.00f, 1.00f};
    colors[ImGuiCol_ChildBg]              = (ImVec4_c){0.00f, 0.00f, 0.00f, 1.00f};
    colors[ImGuiCol_PopupBg]              = (ImVec4_c){0.00f, 0.00f, 0.00f, 1.00f};
    colors[ImGuiCol_Text]                 = (ImVec4_c){1.00f, 1.00f, 1.00f, 1.00f};
    colors[ImGuiCol_Border]               = (ImVec4_c){0.60f, 0.60f, 0.60f, 1.00f};
    colors[ImGuiCol_Separator]            = (ImVec4_c){0.60f, 0.60f, 0.60f, 1.00f};
    colors[ImGuiCol_FrameBg]              = (ImVec4_c){0.00f, 0.00f, 0.00f, 1.00f};
    colors[ImGuiCol_FrameBgHovered]       = (ImVec4_c){0.15f, 0.15f, 0.15f, 1.00f};
    colors[ImGuiCol_FrameBgActive]        = (ImVec4_c){0.25f, 0.25f, 0.25f, 1.00f};
    colors[ImGuiCol_Header]               = (ImVec4_c){0.15f, 0.15f, 0.15f, 1.00f};
    colors[ImGuiCol_HeaderHovered]        = (ImVec4_c){0.25f, 0.25f, 0.25f, 1.00f};
    colors[ImGuiCol_HeaderActive]         = (ImVec4_c){0.35f, 0.35f, 0.35f, 1.00f};
    colors[ImGuiCol_Button]               = (ImVec4_c){0.00f, 0.00f, 0.00f, 1.00f};
    colors[ImGuiCol_ButtonHovered]        = (ImVec4_c){0.25f, 0.25f, 0.25f, 1.00f};
    colors[ImGuiCol_ButtonActive]         = (ImVec4_c){0.35f, 0.35f, 0.35f, 1.00f};
    colors[ImGuiCol_CheckMark]            = (ImVec4_c){1.00f, 1.00f, 1.00f, 1.00f};
    colors[ImGuiCol_SliderGrab]           = (ImVec4_c){1.00f, 1.00f, 1.00f, 1.00f};
    colors[ImGuiCol_SliderGrabActive]     = (ImVec4_c){0.80f, 0.80f, 0.80f, 1.00f};

    ImGui_ImplSDL3_InitForSDLRenderer(cpWindow, cpRenderer);
    ImGui_ImplSDLRenderer3_Init_C(cpRenderer);

    cpOpen = true;
    previewNeedsUpdate = true;
    previewTexture = NULL;
}

void handleControlPanelEvent(const SDL_Event* event) {
    if (!cpOpen) return;

    ImGui_ImplSDL3_ProcessEvent(event);

    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event->window.windowID == SDL_GetWindowID(cpWindow)) {
        closeControlPanel();
    }
}

// Simple file readability check using SDL3 IO
static bool isFileReadable(const char* path) {
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (io) { SDL_CloseIO(io); return true; }
    return false;
}

static void updatePreviewTexture(void) {
    if (previewTexture) {
        SDL_DestroyTexture(previewTexture);
        previewTexture = NULL;
    }

    previewFontLoadFailed = false;

    // Try to load the selected font
    TTF_Font* font = TTF_OpenFont(uiConfig.font, uiConfig.font_size);
    if (!font) {
        // Fallback to default font
        font = TTF_OpenFont("fonts/cascadia.mono.ttf", uiConfig.font_size);
        if (!font) {
            previewFontLoadFailed = true;
            return;
        }
    }

    // Render preview texture
    previewTexture = createTextTexture(cpRenderer, font, "Sample Text Preview", &uiConfig, &previewWidth, &previewHeight);
    TTF_CloseFont(font);
}

ControlPanelStatus updateAndRenderControlPanel(SDL_Renderer* overlayRenderer) {

    ControlPanelStatus status = {0};

    // Regenerate preview if needed
    if (previewNeedsUpdate) {
        updatePreviewTexture();
        previewNeedsUpdate = false;
    }

    // Start ImGui frame
    ImGui_ImplSDLRenderer3_NewFrame_C();
    ImGui_ImplSDL3_NewFrame();
    igNewFrame();

    // ponytail: sync UI if live config changed externally due to CPU fallback
    if (pLiveConfig && savedConfig.use_gpu != pLiveConfig->use_gpu) {
        uiConfig.use_gpu = pLiveConfig->use_gpu;
        savedConfig.use_gpu = pLiveConfig->use_gpu;
    }

    // Check dirty state
    bool isDirty = false;
    if (strcmp(uiConfig.font, savedConfig.font) != 0 ||
        uiConfig.font_size != savedConfig.font_size ||
        uiConfig.outline_thickness != savedConfig.outline_thickness ||
        uiConfig.text_color.r != savedConfig.text_color.r ||
        uiConfig.text_color.g != savedConfig.text_color.g ||
        uiConfig.text_color.b != savedConfig.text_color.b ||
        uiConfig.text_outline_color.r != savedConfig.text_outline_color.r ||
        uiConfig.text_outline_color.g != savedConfig.text_outline_color.g ||
        uiConfig.text_outline_color.b != savedConfig.text_outline_color.b ||
        strcmp(uiConfig.modelPath, savedConfig.modelPath) != 0 ||
        uiConfig.use_gpu != savedConfig.use_gpu) {
        isDirty = true;
    }

    int w, h;
    SDL_GetWindowSize(cpWindow, &w, &h);
    igSetNextWindowPos((ImVec2_c){0, 0}, ImGuiCond_Always, (ImVec2_c){0, 0});
    igSetNextWindowSize((ImVec2_c){(float)w, (float)h}, ImGuiCond_Always);

    igBegin("Preferences", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Status Message & Control Buttons (Centered vertically)
    igAlignTextToFramePadding();
    if (whisperStatusError) {
        igTextColored((ImVec4_c){1.0f, 0.3f, 0.3f, 1.0f}, "%s", whisperStatusMessage);
    } else {
        igTextColored((ImVec4_c){0.3f, 1.0f, 0.3f, 1.0f}, "%s", whisperStatusMessage);
    }

    igSameLine(0.0f, UI_SPACING);
    
    // Align top buttons to the right edge of the window (Total width = 70 + 110 + 100 + 16 = 296)
    float rightAlignX = igGetWindowWidth() - 296.0f - igGetStyle()->WindowPadding.x;
    if (rightAlignX > igGetCursorPosX()) {
        igSetCursorPosX(rightAlignX);
    }

    // Pause/Resume button
    bool currentlyPaused = isAppPaused();
    if (igButton(currentlyPaused ? "Resume" : "Pause", (ImVec2_c){70.0f, 0.0f})) {
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_USER;
        e.user.code = currentlyPaused ? 0 : 1; // 0 = resume, 1 = pause
        SDL_PushEvent(&e);
    }

    igSameLine(0.0f, UI_SPACING);

    // Move Window button
    if (igButton("Move Window", (ImVec2_c){110.0f, 0.0f})) {
        SDL_Window* overlayWinReal = SDL_GetRenderWindow(overlayRenderer);
        if (overlayWinReal) {
            SDL_SetWindowMousePassthrough(overlayWinReal, false);
            SDL_SetWindowBordered(overlayWinReal, true);
        }
    }

    igSameLine(0.0f, UI_SPACING);

    // Close App button
    if (igButton("Close App", (ImVec2_c){100.0f, 0.0f})) {
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&e);
    }

    igSeparator();
    igSpacing();

    // Split layout: Column 0 = Sidebar, Column 1 = Settings Pane
    igColumns(2, "SettingsSidebarLayout", true);
    igSetColumnWidth(0, 130.0f); // Sidebar width
    
    // --- Column 0: Sidebar Navigation ---
    igPushStyleVar_Vec2(ImGuiStyleVar_SelectableTextAlign, (ImVec2_c){0.0f, 0.5f});
    if (igSelectable_Bool("View", cpActivePage == 0, 0, (ImVec2_c){0, 24.0f})) {
        cpActivePage = 0;
    }
    igSpacing();
    if (igSelectable_Bool("Transcription", cpActivePage == 1, 0, (ImVec2_c){0, 24.0f})) {
        cpActivePage = 1;
    }
    igPopStyleVar(1);
    
    igNextColumn();
    
    // --- Column 1: Settings Content ---
    if (cpActivePage == 0) {
        // --- View Page ---
        // 1. Font Selection
        const char* fontDisplayName = getFilenameFromPath(uiConfig.font);
        if (igBeginCombo("Font", fontDisplayName, 0)) {
            if (scannedFontCount == 0) {
                igSelectable_Bool("No item found in folder##empty_font", false, ImGuiSelectableFlags_Disabled, (ImVec2_c){0,0});
            } else {
                for (int i = 0; i < scannedFontCount; i++) {
                    bool isSelected = (strcmp(fontDisplayName, scannedFonts[i]) == 0);
                    char itemDisplay[128];
                    snprintf(itemDisplay, sizeof(itemDisplay), "%s##font%d", scannedFonts[i], i);
                    if (igSelectable_Bool(itemDisplay, isSelected, 0, (ImVec2_c){0, 0})) {
                        snprintf(uiConfig.font, sizeof(uiConfig.font), "fonts/%s", scannedFonts[i]);
                        previewNeedsUpdate = true;
                    }
                    if (isSelected) {
                        igSetItemDefaultFocus();
                    }
                }
            }
            igEndCombo();
        }

        // 2. Font Size
        int tempFontSize = uiConfig.font_size;
        if (igDragInt("Font Size", &tempFontSize, 1.0f, 8, 72, "%d", 0)) {
            uiConfig.font_size = tempFontSize;
            previewNeedsUpdate = true;
        }

        // 3. Outline Thickness
        int tempOutline = uiConfig.outline_thickness;
        if (igDragInt("Outline Thickness", &tempOutline, 0.5f, 0, 20, "%d", 0)) {
            uiConfig.outline_thickness = tempOutline;
            previewNeedsUpdate = true;
        }

        // 4. Color Picking
        float textColor[3] = {
            uiConfig.text_color.r / 255.0f,
            uiConfig.text_color.g / 255.0f,
            uiConfig.text_color.b / 255.0f
        };
        if (igColorEdit3("Text Color", textColor, 0)) {
            uiConfig.text_color.r = (uint8_t)(textColor[0] * 255.0f);
            uiConfig.text_color.g = (uint8_t)(textColor[1] * 255.0f);
            uiConfig.text_color.b = (uint8_t)(textColor[2] * 255.0f);
            previewNeedsUpdate = true;
        }

        float outlineColor[3] = {
            uiConfig.text_outline_color.r / 255.0f,
            uiConfig.text_outline_color.g / 255.0f,
            uiConfig.text_outline_color.b / 255.0f
        };
        if (igColorEdit3("Outline Color", outlineColor, 0)) {
            uiConfig.text_outline_color.r = (uint8_t)(outlineColor[0] * 255.0f);
            uiConfig.text_outline_color.g = (uint8_t)(outlineColor[1] * 255.0f);
            uiConfig.text_outline_color.b = (uint8_t)(outlineColor[2] * 255.0f);
            previewNeedsUpdate = true;
        }

        igSpacing();

        // Live Preview
        igText("Live Preview:");
        ImVec2_c previewPos = igGetCursorScreenPos();
        
        // Draw a dark background rectangle for the preview (no rounding)
        ImDrawList* drawList = igGetWindowDrawList();
        ImDrawList_AddRectFilled(drawList, 
            previewPos, 
            (ImVec2_c){previewPos.x + UI_PREVIEW_BOX_WIDTH, previewPos.y + UI_PREVIEW_BOX_HEIGHT}, 
            IM_COL32(15, 15, 15, 255), 0.0f, 0);

        ImDrawList_AddRect(drawList,
            previewPos,
            (ImVec2_c){previewPos.x + UI_PREVIEW_BOX_WIDTH, previewPos.y + UI_PREVIEW_BOX_HEIGHT},
            IM_COL32(80, 80, 80, 255), 0.0f, 1.0f, 0);

        // Render the texture inside the box
        if (previewFontLoadFailed) {
            igSetCursorScreenPos((ImVec2_c){previewPos.x + 10.0f, previewPos.y + 10.0f});
            igTextColored((ImVec4_c){1.0f, 0.3f, 0.3f, 1.0f}, "Preview unavailable (No valid fonts found)");
        } else if (previewTexture) {
            // Clamp and scale preview if it exceeds bounds to prevent overflow
            float maxPreviewW = UI_PREVIEW_BOX_WIDTH - 20.0f;
            float maxPreviewH = UI_PREVIEW_BOX_HEIGHT - 20.0f;
            float displayW = previewWidth;
            float displayH = previewHeight;

            if (displayW > maxPreviewW || displayH > maxPreviewH) {
                float scaleX = maxPreviewW / displayW;
                float scaleY = maxPreviewH / displayH;
                float scale = (scaleX < scaleY) ? scaleX : scaleY;
                displayW *= scale;
                displayH *= scale;
            }

            // Center the scaled preview texture inside the box
            float startX = previewPos.x + (UI_PREVIEW_BOX_WIDTH - displayW) / 2.0f;
            float startY = previewPos.y + (UI_PREVIEW_BOX_HEIGHT - displayH) / 2.0f;

            igSetCursorScreenPos((ImVec2_c){startX, startY});
            ImTextureRef_c texRef = { NULL, (ImTextureID)(intptr_t)previewTexture };
            igImage(texRef, (ImVec2_c){displayW, displayH}, (ImVec2_c){0,0}, (ImVec2_c){1,1});
        }

        // Dummy element to advance the cursor past the preview box
        igSetCursorScreenPos((ImVec2_c){previewPos.x, previewPos.y + UI_PREVIEW_BOX_HEIGHT + UI_SPACING});

    } else if (cpActivePage == 1) {
        // --- Transcription Page ---
        // 5. Model Selection
        const char* modelDisplayName = getFilenameFromPath(uiConfig.modelPath);
        if (igBeginCombo("Model", modelDisplayName, 0)) {
            if (scannedModelCount == 0) {
                igSelectable_Bool("No item found in folder##empty_model", false, ImGuiSelectableFlags_Disabled, (ImVec2_c){0,0});
            } else {
                for (int i = 0; i < scannedModelCount; i++) {
                    bool isSelected = (strcmp(modelDisplayName, scannedModels[i]) == 0);
                    char itemDisplay[128];
                    snprintf(itemDisplay, sizeof(itemDisplay), "%s##model%d", scannedModels[i], i);
                    if (igSelectable_Bool(itemDisplay, isSelected, 0, (ImVec2_c){0,0})) {
                        snprintf(uiConfig.modelPath, sizeof(uiConfig.modelPath), "models/%s", scannedModels[i]);
                    }
                    if (isSelected) {
                        igSetItemDefaultFocus();
                    }
                }
            }
            igEndCombo();
        }

        igSpacing();
        // GPU Toggle
        igCheckbox("Use GPU (Vulkan)", &uiConfig.use_gpu);
    }
    
    igColumns(1, NULL, false); // Restore to single column
    
    // Force the action buttons to always sit at the bottom of the window
    float windowHeight = igGetWindowHeight();
    float paddingY = igGetStyle()->WindowPadding.y;
    float footerStartY = windowHeight - paddingY - 30.0f - 20.0f; // 30px button height + 20px margin
    
    igSetCursorPosY(footerStartY);
    igSeparator();
    igSpacing();

    // 7. Buttons (Save & Load Defaults - right-aligned, Load Defaults on Left)
    float btnStartX = igGetWindowWidth() - (UI_BUTTON_WIDTH * 2.0f) - UI_SPACING - igGetStyle()->WindowPadding.x;
    if (btnStartX < 0.0f) btnStartX = 0.0f;

    // Render error message to the left of the buttons
    if (cpHasError) {
        igPushStyleColor_Vec4(ImGuiCol_Text, (ImVec4_c){1.0f, 0.3f, 0.3f, 1.0f});
        igAlignTextToFramePadding();
        igBeginGroup();
        if (strchr(cpErrorMessage, '\n') != NULL) {
            igSetCursorPosY(igGetCursorPosY() - igGetTextLineHeight() * 0.5f);
        }
        igText("%s", cpErrorMessage);
        igEndGroup();
        igPopStyleColor(1);
        igSameLine(0.0f, -1.0f);
    }

    igSetCursorPosX(btnStartX);

    // Load Defaults Button
    if (igButton("Load Defaults", (ImVec2_c){UI_BUTTON_WIDTH, 0.0f})) {
        uiConfig = loadDefaultConfig();
        previewNeedsUpdate = true;
        cpHasError = false;
        cpErrorMessage[0] = '\0';
    }

    igSameLine(0.0f, UI_SPACING);

    // Save Button
    if (!isDirty) {
        igBeginDisabled(true);
        igButton("Saved", (ImVec2_c){UI_BUTTON_WIDTH, 0.0f});
        igEndDisabled();
    } else {
        if (igButton("Save", (ImVec2_c){UI_BUTTON_WIDTH, 0.0f})) {
            char fontError[128] = "";
            char modelError[128] = "";
            bool fontOk = isFileReadable(uiConfig.font);
            bool modelOk = isFileReadable(uiConfig.modelPath);

            if (!fontOk) {
                SDL_strlcpy(fontError, "Error: Font unreadable, using fallback", sizeof(fontError));
            }
            if (!modelOk) {
                SDL_strlcpy(modelError, "Error: Model unreadable, using fallback", sizeof(modelError));
            }

            if (!fontOk || !modelOk) {
                cpHasError = true;
                if (!fontOk && !modelOk) {
                    snprintf(cpErrorMessage, sizeof(cpErrorMessage), "%s\n%s", fontError, modelError);
                } else if (!fontOk) {
                    SDL_strlcpy(cpErrorMessage, fontError, sizeof(cpErrorMessage));
                } else {
                    SDL_strlcpy(cpErrorMessage, modelError, sizeof(cpErrorMessage));
                }
            } else if (saveConfig(&uiConfig)) {
                if (pLiveConfig) {
                    *pLiveConfig = uiConfig;
                }
                status.configSaved = true;
                if (strcmp(uiConfig.modelPath, savedConfig.modelPath) != 0 ||
                    uiConfig.use_gpu != savedConfig.use_gpu) {
                    status.modelChanged = true;
                }
                savedConfig = uiConfig;
                SDL_strlcpy(whisperStatusMessage, "Status: Active (Config Saved)", sizeof(whisperStatusMessage));
                whisperStatusError = false;
                cpHasError = false;
                cpErrorMessage[0] = '\0';
            } else {
                cpHasError = true;
                SDL_strlcpy(cpErrorMessage, "Error: Failed to write config", sizeof(cpErrorMessage));
            }
        }
    }

    igEnd();

    // Render ImGui
    igRender();
    
    SDL_SetRenderDrawColor(cpRenderer, 30, 30, 30, 255);
    SDL_RenderClear(cpRenderer);
    ImGui_ImplSDLRenderer3_RenderDrawData_C(igGetDrawData(), cpRenderer);
    SDL_RenderPresent(cpRenderer);

    return status;
}

void closeControlPanel(void) {
    if (!cpOpen) return;

    // Shutdown ImGui
    ImGui_ImplSDLRenderer3_Shutdown_C();
    ImGui_ImplSDL3_Shutdown();
    igDestroyContext(NULL);

    if (previewTexture) {
        SDL_DestroyTexture(previewTexture);
        previewTexture = NULL;
    }

    // Destroy window and renderer
    if (cpRenderer) {
        SDL_DestroyRenderer(cpRenderer);
        cpRenderer = NULL;
    }
    if (cpWindow) {
        SDL_DestroyWindow(cpWindow);
        cpWindow = NULL;
    }

    cpOpen = false;
}


bool isControlPanelOpen(void) {
    return cpOpen;
}

void setControlPanelWhisperError(bool error, const char* message) {
    whisperStatusError = error;
    if (message) {
        SDL_strlcpy(whisperStatusMessage, message, sizeof(whisperStatusMessage));
    } else {
        SDL_strlcpy(whisperStatusMessage, error ? "Status: Error" : "Status: Active", sizeof(whisperStatusMessage));
    }
}
