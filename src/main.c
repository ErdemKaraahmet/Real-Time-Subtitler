#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "configManager.h"
#include "windowManager.h"
#include "textTexture.h"
#include "audioCapture.h"
#include "whisperEngine.h"
#include "trayManager.h"
#include "controlPanel.h"
#include "modelManager.h"
#include "appEvents.h"
#include "utils.h"

#define CHUNK_LENGTH_SECONDS 2
#define SAMPLE_RATE 16000                                // 16Khz
#define SAMPLE_SIZE (CHUNK_LENGTH_SECONDS * SAMPLE_RATE) // CHUNK_LENGTH_SECONDS second * sample rate = 32000 frames

// shared state between threads
static char subtitleText[124] = "";
static float audioChunk[SAMPLE_SIZE];
static bool chunkReady = false;
static bool textUpdated = false;
static bool paused = false;
static SDL_Mutex *subtitleMutex;
static SDL_Mutex *audioMutex;
static Uint64 lastTextUpdateTime = 0; // timestamp of the last whisper text update (ms)
static bool done = false;
static volatile bool modelReloadRequested = false;

static SubtitleToken outputTokens[1024];
static int tokenNum;

static SDL_Thread *wThread = NULL;

#ifdef RTS_MONKEY_TEST
static SDL_Thread *monkeyThread = NULL;
#endif

int whisperThread(void *data);

void handleEvents(bool *done, bool *needsRedraw, int timeout, AppConfig *config);

static void handleModelReload(AppConfig *config);

static void resetSubtitleState(void);

int main(int argc, char *argv[]) {
#ifdef RTS_MONKEY_TEST
    utilsParseMonkeyArgs(argc, argv);
#else
    (void)argc;
    (void)argv;
#endif
#ifdef _WIN32
    // Hide the console when double-clicked from Explorer.
    // When launched from a terminal, other processes share the console so count > 1.
    DWORD processList[2];
    if (GetConsoleProcessList(processList, 2) <= 1) {
        FreeConsole();
    }
#endif

    // Initialize SDL and TTF
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    modelManagerInit();

    // Load user config
    AppConfig config_obj = loadDefaultConfig();
    AppConfig *config = &config_obj;
    ConfigLoadStatus loadStatus = loadConfig(config);
    switch (loadStatus) {
    case CONFIG_LOAD_OK:
        SDL_Log("Config loaded.");
        break;
    case CONFIG_LOAD_PARSE_ERROR:
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Config file was corrupted, backed up to config.json.bak and recreated with defaults.");
        saveConfig(config);
        break;
    case CONFIG_LOAD_FILE_NOT_FOUND:
    default:
        SDL_Log("Config file not found, creating config.json with defaults.");
        saveConfig(config);
        break;
    }

    SDL_Log("Initializing audio capture...");
    initAndStartAudio();
    SDL_Log("Audio capture initialized.");

    SDL_Log("Loading whisper model: %s", config->modelPath);
    bool previousGpu = config->use_gpu;
    if (whisperInit(config->modelPath, &config->use_gpu)) {
        SDL_Log("Whisper model loaded");
        if (previousGpu != config->use_gpu) {
            saveConfig(config); // Save the CPU fallback configuration
        }
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load whisper model: %s", config->modelPath);
        openControlPanelToTranscriptionWithError(
            config, "No Whisper model is loaded.\n\nPlease select or download a model from the list above to start subtitling.");
        setControlPanelWhisperError(true, "Status: Whisper Offline (Model Load Failed)");
    }

    // Create a transparent window
    SDL_Log("Creating window...");
    if (!initWindow()) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return 1;
    }
    setWindowCenter(config->window_x, config->window_y);

    SDL_Log("Initializing system tray...");
    initTray();

    if (config->open_control_panel_on_startup && !isControlPanelOpen()) {
        openControlPanel(config);
    }

    // Load a font
    char initialFontPath[512];
    utilsResolvePath(initialFontPath, sizeof(initialFontPath), config->font);
    TTF_Font *font = TTF_OpenFont(initialFontPath, (float)config->font_size);
    if (!font) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load font %s: %s", config->font, SDL_GetError());
        return 1;
    }

    subtitleMutex = SDL_CreateMutex();
    audioMutex = SDL_CreateMutex();
    wThread = SDL_CreateThread(whisperThread, "whisper", config);
    if (!wThread) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create whisper thread: %s", SDL_GetError());
    }

#ifdef RTS_MONKEY_TEST
    if (utilsIsMonkeyModeEnabled()) {
        monkeyThread = SDL_CreateThread(utilsRunMonkeyEventLoop, "MonkeyThread", (void *)&done);
    }
#endif

    done = false;
    bool needsRedraw = true;
    while (!done) {
        RTS_LockMutex(audioMutex);
        if (audioChunkReady(SAMPLE_SIZE) && !chunkReady) {
            if (getAudioChunk(audioChunk, SAMPLE_SIZE)) {
                chunkReady = true; // signal the whisper thread
            }
        }
        RTS_UnlockMutex(audioMutex);

        RTS_LockMutex(subtitleMutex);
        if (textUpdated) {
            if (!strcmp(subtitleText, " [BLANK_AUDIO]"))
                subtitleText[0] = '\0'; // whisper outputs " [BLANK_AUDIO]" on empty audio, to not print it exactly
            updateSubtitleText(font, outputTokens, tokenNum, config);

            textUpdated = false;
            needsRedraw = true;
            lastTextUpdateTime = SDL_GetTicks();
        }
        RTS_UnlockMutex(subtitleMutex);

        modelManagerPoll();
        bool cpOpen = isControlPanelOpen();
        bool snapBusy = updateWindowSnap();

        // Wait for events. Timeout is 16ms when Control Panel is open or a snap drag/animation
        // is in progress, 100ms when idle/stationary.
        int timeout = (cpOpen || snapBusy) ? 16 : 100;
        handleEvents(&done, &needsRedraw, timeout, config);

        // Clear the subtitle overlay if no new text has arrived within the timeout
        if (hasSubtitleText() && lastTextUpdateTime > 0 && SDL_GetTicks() - lastTextUpdateTime > (Uint64)(CHUNK_LENGTH_SECONDS + 1) * 1000) {
            resetSubtitleState();
            needsRedraw = true;
        }

        if (needsRedraw) {
            renderSubtitleWindow();
            needsRedraw = false;
        }

        // Render Control Panel if open
        if (isControlPanelOpen()) {
            // Snapshot font config before the CP call may modify it via pLiveConfig
            char prevFont[512];
            int prevFontSize = config->font_size;
            SDL_strlcpy(prevFont, config->font, sizeof(prevFont));

            ControlPanelStatus cpStatus = updateAndRenderControlPanel(paused);
            if (cpStatus.configSaved) {
                // Only reload font if the font path or size actually changed
                if (strcmp(config->font, prevFont) != 0 || config->font_size != prevFontSize) {
                    char reloadedFontPath[512];
                    utilsResolvePath(reloadedFontPath, sizeof(reloadedFontPath), config->font);
                    TTF_Font *new_font = TTF_OpenFont(reloadedFontPath, (float)config->font_size);
                    if (new_font) {
                        if (font)
                            TTF_CloseFont(font);
                        font = new_font;
                    } else {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to reload font %s: %s. Reverting to previous font.", config->font,
                                     SDL_GetError());
                        // Revert config to previous working font settings
                        SDL_strlcpy(config->font, prevFont, sizeof(config->font));
                        config->font_size = prevFontSize;
                    }
                }
                // Trigger subtitle redraw only if a subtitle is currently active
                RTS_LockMutex(subtitleMutex);
                if (subtitleText[0] != '\0' && hasSubtitleText()) {
                    textUpdated = true;
                }
                RTS_UnlockMutex(subtitleMutex);
            }
            if (cpStatus.modelChanged) {
                modelReloadRequested = true;
            }
        }

        // If the model is being reloaded, show a spinner in the Control Panel status bar
        if (modelReloadRequested) {
            static Uint64 lastSpinnerTime = 0;
            static int spinnerIdx = 0;
            Uint64 now = SDL_GetTicks();
            if (now - lastSpinnerTime >= 100) {
                lastSpinnerTime = now;
                const char spinner[] = "|/-\\";
                char statusBuf[64];
                (void)SDL_snprintf(statusBuf, sizeof(statusBuf), "Status: Reloading Model... [%c]", spinner[spinnerIdx]);
                setControlPanelWhisperError(true, statusBuf);
                spinnerIdx = (spinnerIdx + 1) % 4;
            }
        }

        SDL_Delay(1000 / 60); // Limit to 60 FPS
    }

    // Save window center position on app shutdown
    getWindowCenter(&config->window_x, &config->window_y);
    saveConfig(config);

    // Close and destroy the window
    destroyWindow();

    // Clean up
    closeControlPanel();
    modelManagerShutdown();

#ifdef RTS_MONKEY_TEST
    if (monkeyThread) {
        SDL_WaitThread(monkeyThread, NULL);
    }
#endif

    if (wThread) {
        SDL_WaitThread(wThread, NULL);
    }
    whisperFree();
    cleanupAudio();
    destroyTray();
    SDL_DestroyMutex(subtitleMutex);
    SDL_DestroyMutex(audioMutex);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    return 0;
}

void handleEvents(bool *pDone, bool *needsRedraw, int timeout, AppConfig *config) {
    SDL_Event event;
    if (SDL_WaitEventTimeout(&event, timeout)) {
        do {
            // Pass event to Control Panel
            handleControlPanelEvent(&event);

            if (event.type == SDL_EVENT_QUIT) {
                *pDone = true;
            }

            if (event.type == SDL_EVENT_USER) {
                if (event.user.code == APP_EVENT_PAUSE) {
                    paused = true;
                    pauseAudio();
                    setTrayPauseState(true);
                    // Immediately clear the on-screen text
                    resetSubtitleState();
                } else if (event.user.code == APP_EVENT_RESUME) {
                    paused = false;
                    resumeAudio();
                    setTrayPauseState(false);
                } else if (event.user.code == APP_EVENT_MOVE_WINDOW) {
                    setWindowMoveMode(true);
                } else if (event.user.code == APP_EVENT_OPEN_CONTROL) {
                    openControlPanel(config);
                }
                *needsRedraw = true;
            }

            if (event.type == SDL_EVENT_WINDOW_MOVED && isWindowID(event.window.windowID)) {
                handleWindowMovedEvent();
                *needsRedraw = true;
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST && isWindowID(event.window.windowID)) {
                setWindowMoveMode(false);
                *needsRedraw = true;
            }
            if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
                *needsRedraw = true;
            }
        } while (SDL_PollEvent(&event));
    }
}

int whisperThread(void *data) {
    const AppConfig *config = (AppConfig *)data;
    static float localChunk[SAMPLE_SIZE];
    while (!done) {
        if (modelReloadRequested) {
            modelReloadRequested = false;
            handleModelReload((AppConfig *)config);
        }

        bool hasChunk = false;
        RTS_LockMutex(audioMutex);
        if (chunkReady) {
            if (!paused) {
                memcpy(localChunk, audioChunk, sizeof(localChunk));
                hasChunk = true;
            }
            chunkReady = false;
        }
        RTS_UnlockMutex(audioMutex);

        if (hasChunk) {
            char localText[512] = {0};
            int localTokenNum = 0;
            SubtitleToken localTokens[1024];

            whisperProcess(localChunk, SAMPLE_SIZE, localText, sizeof(localText), config->cpu_threads, config->language, localTokens, &localTokenNum);

            RTS_LockMutex(subtitleMutex);
            SDL_strlcpy(subtitleText, localText, sizeof(subtitleText));
            tokenNum = localTokenNum;
            memcpy(outputTokens, localTokens, (size_t)localTokenNum * sizeof(SubtitleToken));
            textUpdated = true;
            RTS_UnlockMutex(subtitleMutex);

            // Wake up the main event loop immediately
            SDL_Event event;
            SDL_zero(event);
            event.type = SDL_EVENT_USER;
            event.user.code = APP_EVENT_TEXT_UPDATED;
            SDL_PushEvent(&event);
        }

        SDL_Delay(10);
    }
    return 0;
}

static void handleModelReload(AppConfig *config) {
    whisperFree();
    bool prevGpu = config->use_gpu;
    if (whisperInit(config->modelPath, &config->use_gpu)) {
        SDL_Log("Whisper model reloaded: %s (GPU: %s)", config->modelPath, config->use_gpu ? "yes" : "no");
        if (prevGpu != config->use_gpu) {
            saveConfig(config);
        }
        if (config->use_gpu) {
            setControlPanelWhisperError(false, "Status: Active (GPU Enabled)");
        } else if (prevGpu && !config->use_gpu) {
            setControlPanelWhisperError(true, "Status: Active (Vulkan Failed - CPU Fallback)");
        } else {
            setControlPanelWhisperError(false, "Status: Active (CPU Only)");
        }
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to reload Whisper model: %s", config->modelPath);
        setControlPanelWhisperError(true, "Status: Whisper Offline (Model Load Failed)");
        openControlPanelToTranscriptionWithError(config, "Failed to reload the Whisper model.\n\nPlease select or download another model.");
    }
}

static void resetSubtitleState(void) {
    RTS_LockMutex(subtitleMutex);
    subtitleText[0] = '\0';
    tokenNum = 0;
    RTS_UnlockMutex(subtitleMutex);
    clearSubtitleText();
}
