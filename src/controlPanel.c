#include "controlPanel_internal.h"

// External C++ declarations from our bridge
extern bool ImGui_ImplSDLRenderer3_Init_C(SDL_Renderer *renderer);
extern void ImGui_ImplSDLRenderer3_Shutdown_C(void);
extern void ImGui_ImplSDLRenderer3_NewFrame_C(void);
extern void ImGui_ImplSDLRenderer3_RenderDrawData_C(ImDrawData *draw_data, SDL_Renderer *renderer);

// UI Styling Constants
const float UI_WINDOW_WIDTH = 710.0f;
const float UI_PADDING = 12.0f;
const float UI_SPACING = 8.0f;
const float UI_BUTTON_WIDTH = 120.0f;
const float UI_PREVIEW_BOX_WIDTH = 556.0f; // 580 - 24
const float UI_PREVIEW_BOX_HEIGHT = 70.0f;
const float UI_WINDOW_HEIGHT = 450.0f;

int g_DeleteTargetIndex = -1;

// Window and Renderer state
SDL_Window *cpWindow = NULL;
SDL_Renderer *cpRenderer = NULL;
bool cpOpen = false;

// Scanned items
char scannedFonts[MAX_ITEMS][256];
int scannedFontCount = 0;

// UI configuration state
AppConfig *pLiveConfig = NULL;
AppConfig uiConfig;
AppConfig savedConfig; // to track dirty state
bool modelChanged = false;
char whisperStatusMessage[256] = "Status: Active";
bool whisperStatusError = false;
int cpActivePage = 0; // 0 = View, 1 = Transcription

// Global error popup state
char globalUiErrorMessage[512] = "";
bool showGlobalUiErrorPopup = false;

// Preview state
SDL_Texture *previewTexture = NULL;
float previewWidth = 0.0f;
float previewHeight = 0.0f;
bool previewNeedsUpdate = true;
bool previewFontLoadFailed = false;

void triggerGlobalError(const char *fmt, ...) {
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        (void)vsnprintf(globalUiErrorMessage, sizeof(globalUiErrorMessage), fmt, args);
        va_end(args);
        showGlobalUiErrorPopup = true;
    }
}

const char *getFilenameFromPath(const char *path) {
    const char *lastSlash = strrchr(path, '/');
    const char *lastBackslash = strrchr(path, '\\');
    const char *filename = path;
    if (lastSlash && lastSlash > filename)
        filename = lastSlash + 1;
    if (lastBackslash && lastBackslash > filename)
        filename = lastBackslash + 1;
    return filename;
}

static SDL_EnumerationResult SDLCALL scanFontsCallback(void *userdata, const char *dirname, const char *fname) {
    (void)userdata;
    (void)dirname;
    if (scannedFontCount < MAX_ITEMS) {
        size_t len = strlen(fname);
        if (len > 4 && SDL_strcasecmp(fname + len - 4, ".ttf") == 0) {
            SDL_strlcpy(scannedFonts[scannedFontCount], fname, sizeof(scannedFonts[scannedFontCount]));
            scannedFontCount++;
        }
    }
    return SDL_ENUM_CONTINUE;
}

void openControlPanelToTranscriptionWithError(AppConfig *liveConfig, const char *errorMessage) {
    cpActivePage = 1;
    openControlPanel(liveConfig);
    if (errorMessage) {
        triggerGlobalError("%s", errorMessage);
    }
}

void openControlPanel(AppConfig *liveConfig) {
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

    char path[512];
    utilsResolvePath(path, sizeof(path), "fonts");
    SDL_EnumerateDirectory(path, scanFontsCallback, NULL);

    // Create window & renderer
    cpWindow = SDL_CreateWindow("RTS Control Panel", (int)UI_WINDOW_WIDTH, (int)UI_WINDOW_HEIGHT, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!cpWindow) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Control Panel window: %s", SDL_GetError());
        return;
    }

    // Set window icon
    char iconPath[512];
    utilsResolvePath(iconPath, sizeof(iconPath), "rts_icon.png");
    SDL_Surface *icon = SDL_LoadPNG(iconPath);
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
    ImGuiIO *io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load Cascadia Font for UI
    char uiFontPath[512];
    utilsResolvePath(uiFontPath, sizeof(uiFontPath), "fonts/cascadia.mono.ttf");
    ImFontAtlas_AddFontFromFileTTF(io->Fonts, uiFontPath, 16.0f, NULL, NULL);

    // Setup style
    igStyleColorsDark(NULL);
    ImGuiStyle *style = igGetStyle();
    style->WindowPadding = (ImVec2_c){UI_PADDING, UI_PADDING};
    style->ItemSpacing = (ImVec2_c){UI_SPACING, UI_SPACING};
    style->FramePadding = (ImVec2_c){6.0f, 6.0f};
    style->ButtonTextAlign = (ImVec2_c){0.5f, 0.40f};
    style->SelectableTextAlign = (ImVec2_c){0.0f, 0.40f};

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
    ImVec4 *colors = style->Colors;
    colors[ImGuiCol_WindowBg] = (ImVec4_c){0.00f, 0.00f, 0.00f, 1.00f};
    colors[ImGuiCol_ChildBg] = (ImVec4_c){0.00f, 0.00f, 0.00f, 1.00f};
    colors[ImGuiCol_PopupBg] = (ImVec4_c){0.00f, 0.00f, 0.00f, 1.00f};
    colors[ImGuiCol_Text] = (ImVec4_c){1.00f, 1.00f, 1.00f, 1.00f};
    colors[ImGuiCol_Border] = (ImVec4_c){0.60f, 0.60f, 0.60f, 1.00f};
    colors[ImGuiCol_Separator] = (ImVec4_c){0.60f, 0.60f, 0.60f, 1.00f};
    colors[ImGuiCol_FrameBg] = (ImVec4_c){0.00f, 0.00f, 0.00f, 1.00f};
    colors[ImGuiCol_FrameBgHovered] = (ImVec4_c){0.15f, 0.15f, 0.15f, 1.00f};
    colors[ImGuiCol_FrameBgActive] = (ImVec4_c){0.25f, 0.25f, 0.25f, 1.00f};
    colors[ImGuiCol_Header] = (ImVec4_c){0.15f, 0.15f, 0.15f, 1.00f};
    colors[ImGuiCol_HeaderHovered] = (ImVec4_c){0.25f, 0.25f, 0.25f, 1.00f};
    colors[ImGuiCol_HeaderActive] = (ImVec4_c){0.35f, 0.35f, 0.35f, 1.00f};
    colors[ImGuiCol_Button] = (ImVec4_c){0.00f, 0.00f, 0.00f, 1.00f};
    colors[ImGuiCol_ButtonHovered] = (ImVec4_c){0.25f, 0.25f, 0.25f, 1.00f};
    colors[ImGuiCol_ButtonActive] = (ImVec4_c){0.35f, 0.35f, 0.35f, 1.00f};
    colors[ImGuiCol_CheckMark] = (ImVec4_c){1.00f, 1.00f, 1.00f, 1.00f};
    colors[ImGuiCol_SliderGrab] = (ImVec4_c){1.00f, 1.00f, 1.00f, 1.00f};
    colors[ImGuiCol_SliderGrabActive] = (ImVec4_c){0.80f, 0.80f, 0.80f, 1.00f};
    colors[ImGuiCol_ModalWindowDimBg] = (ImVec4_c){0.00f, 0.00f, 0.00f, 0.60f};
    colors[ImGuiCol_TitleBg] = (ImVec4_c){0.00f, 0.00f, 0.00f, 1.00f};
    colors[ImGuiCol_TitleBgActive] = (ImVec4_c){0.15f, 0.15f, 0.15f, 1.00f};

    ImGui_ImplSDL3_InitForSDLRenderer(cpWindow, cpRenderer);
    ImGui_ImplSDLRenderer3_Init_C(cpRenderer);

    cpOpen = true;
    previewNeedsUpdate = true;
    previewTexture = NULL;
    modelManagerStartFetchCatalog();
}

void handleControlPanelEvent(const SDL_Event *event) {
    if (!cpOpen)
        return;

    ImGui_ImplSDL3_ProcessEvent(event);

    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event->window.windowID == SDL_GetWindowID(cpWindow)) {
        closeControlPanel();
    }
}

static void renderHeaderAndSidebar(const bool isPaused) {
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
    if (igButton(isPaused ? "Resume" : "Pause", (ImVec2_c){70.0f, 0.0f})) {
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_USER;
        e.user.code = isPaused ? APP_EVENT_RESUME : APP_EVENT_PAUSE;
        SDL_PushEvent(&e);
    }

    igSameLine(0.0f, UI_SPACING);

    // Move Window button
    if (igButton("Move Window", (ImVec2_c){110.0f, 0.0f})) {
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_USER;
        e.user.code = APP_EVENT_MOVE_WINDOW;
        SDL_PushEvent(&e);
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
    igSetCursorPosY(igGetCursorPosY() + 1.0f);
    igPushStyleVar_Vec2(ImGuiStyleVar_SelectableTextAlign, (ImVec2_c){0.0f, 0.5f});
    if (igSelectable_Bool("View", cpActivePage == 0, 0, (ImVec2_c){0, 24.0f})) {
        cpActivePage = 0;
    }
    igSpacing();
    if (igSelectable_Bool("Transcription", cpActivePage == 1, 0, (ImVec2_c){0, 24.0f})) {
        cpActivePage = 1;
    }
    igSpacing();
    if (igSelectable_Bool("System", cpActivePage == 2, 0, (ImVec2_c){0, 24.0f})) {
        cpActivePage = 2;
    }
    igPopStyleVar(1);

    igNextColumn();
}

ControlPanelStatus updateAndRenderControlPanel(bool isPaused) {
    ControlPanelStatus status = {0};
    const char *activeModelFilename = getFilenameFromPath(savedConfig.modelPath);
    bool triggerDeletePopup = false;

    // Regenerate preview if needed
    if (previewNeedsUpdate) {
        updatePreviewTexture();
        previewNeedsUpdate = false;
    }

    // Start ImGui frame
    ImGui_ImplSDLRenderer3_NewFrame_C();
    ImGui_ImplSDL3_NewFrame();
    igNewFrame();

    // Sync UI if live config changed externally due to CPU fallback
    if (pLiveConfig && savedConfig.use_gpu != pLiveConfig->use_gpu) {
        uiConfig.use_gpu = pLiveConfig->use_gpu;
        savedConfig.use_gpu = pLiveConfig->use_gpu;
    }

    // Check dirty state
    bool isDirty = whisperStatusError;
    if (strcmp(uiConfig.font, savedConfig.font) != 0 || uiConfig.font_size != savedConfig.font_size ||
        uiConfig.outline_thickness != savedConfig.outline_thickness || uiConfig.text_color.r != savedConfig.text_color.r ||
        uiConfig.text_color.g != savedConfig.text_color.g || uiConfig.text_color.b != savedConfig.text_color.b ||
        uiConfig.text_outline_color.r != savedConfig.text_outline_color.r || uiConfig.text_outline_color.g != savedConfig.text_outline_color.g ||
        uiConfig.text_outline_color.b != savedConfig.text_outline_color.b || strcmp(uiConfig.modelPath, savedConfig.modelPath) != 0 ||
        uiConfig.use_gpu != savedConfig.use_gpu || uiConfig.cpu_threads != savedConfig.cpu_threads ||
        uiConfig.display_mode != savedConfig.display_mode || strcmp(uiConfig.language, savedConfig.language) != 0) {
        isDirty = true;
    }

    int w, h;
    SDL_GetWindowSize(cpWindow, &w, &h);
    igSetNextWindowPos((ImVec2_c){0, 0}, ImGuiCond_Always, (ImVec2_c){0, 0});
    igSetNextWindowSize((ImVec2_c){(float)w, (float)h}, ImGuiCond_Always);

    igBegin("Control Panel", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    renderHeaderAndSidebar(isPaused);

    if (cpActivePage == 0) {
        renderViewPage();
    } else if (cpActivePage == 1) {
        renderTranscriptionPage(activeModelFilename, &triggerDeletePopup);
    } else if (cpActivePage == 2) {
        renderSystemPage();
    }

    igColumns(1, NULL, false); // Restore to single column

    renderFooter(&status, isDirty);
    renderModals(activeModelFilename);

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
    if (!cpOpen)
        return;

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

void setControlPanelWhisperError(bool error, const char *message) {
    whisperStatusError = error;
    if (message) {
        SDL_strlcpy(whisperStatusMessage, message, sizeof(whisperStatusMessage));
    } else {
        SDL_strlcpy(whisperStatusMessage, error ? "Status: Error" : "Status: Active", sizeof(whisperStatusMessage));
    }
}
