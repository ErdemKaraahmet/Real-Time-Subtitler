#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string.h>

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

#define CHUNK_LENGTH_SECONDS 2
#define SAMPLE_RATE 16000                                // 16Khz
#define SAMPLE_SIZE (CHUNK_LENGTH_SECONDS * SAMPLE_RATE) // CHUNK_LENGTH_SECONDS second * sample rate = 32000 frames

// shared state between threads
static char subtitleText[124] = "";
static float audioChunk[SAMPLE_SIZE];
static bool chunkReady = false;
static bool textUpdated = false;
static bool paused = false;
static SDL_Mutex *textMutex;
static Uint64 lastTextUpdateTime = 0; // timestamp of the last whisper text update (ms)
static bool done = false;

static SubtitleToken outputTokens[1024];
static int tokenNum;

int whisperThread(void *data);

void handleEvents(bool *done, bool *needsRedraw, int timeout, AppConfig *config);

static void handleModelReload(AppConfig *config);

int main(void) {
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
    if (!initWindow(240, 80)) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return 1;
    }
    setWindowCenter(config->window_x, config->window_y);

    SDL_Log("Initializing system tray...");
    initTray();

    // Load a font
    TTF_Font *font = TTF_OpenFont(config->font, (float)config->font_size);
    if (!font) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load configured font: %s. Trying fallback default font.", SDL_GetError());
        SDL_strlcpy(config->font, "fonts/cascadia.mono.ttf", sizeof(config->font));
        font = TTF_OpenFont(config->font, (float)config->font_size);
        if (!font) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load fallback font: %s", SDL_GetError());
            return 1;
        }
    }

    textMutex = SDL_CreateMutex();
    SDL_Thread *wThread = SDL_CreateThread(whisperThread, "whisper", config);
    if (!wThread) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create whisper thread: %s", SDL_GetError());
    }

    done = false;
    bool needsRedraw = true;
    while (!done) {
        if (audioChunkReady(SAMPLE_SIZE) && !chunkReady) {
            if (getAudioChunk(audioChunk, SAMPLE_SIZE)) {
                chunkReady = true; // signal the whisper thread
            }
        }

        if (textUpdated) {
            SDL_LockMutex(textMutex);
            if (!strcmp(subtitleText, " [BLANK_AUDIO]"))
                subtitleText[0] = '\0'; // whisper outputs " [BLANK_AUDIO]" on empty audio, to not print it exactly
            updateSubtitleText(font, outputTokens, tokenNum, config);

            textUpdated = false;
            needsRedraw = true;
            lastTextUpdateTime = SDL_GetTicks();
            SDL_UnlockMutex(textMutex);
        }

        modelManagerPoll();
        bool cpOpen = isControlPanelOpen();

        // Wait for events. Timeout is 16ms when Control Panel is open, 100ms when idle/stationary.
        int timeout = (cpOpen) ? 16 : 100;
        handleEvents(&done, &needsRedraw, timeout, config);

        // Clear the subtitle overlay if no new text has arrived within the timeout
        if (hasSubtitleText() && lastTextUpdateTime > 0 && SDL_GetTicks() - lastTextUpdateTime > (Uint64)(CHUNK_LENGTH_SECONDS + 1) * 1000) {
            SDL_LockMutex(textMutex);
            subtitleText[0] = '\0';
            tokenNum = 0;
            SDL_UnlockMutex(textMutex);

            clearSubtitleText();
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
                    TTF_Font *new_font = TTF_OpenFont(config->font, (float)config->font_size);
                    if (new_font) {
                        if (font)
                            TTF_CloseFont(font);
                        font = new_font;
                    } else {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to reload font: %s", SDL_GetError());
                        // Revert config to previous working font settings
                        SDL_strlcpy(config->font, prevFont, sizeof(config->font));
                        config->font_size = prevFontSize;
                    }
                }
                // Trigger subtitle redraw only if a subtitle is currently active
                SDL_LockMutex(textMutex);
                if (subtitleText[0] != '\0' && hasSubtitleText()) {
                    textUpdated = true;
                }
                SDL_UnlockMutex(textMutex);
            }
            if (cpStatus.modelChanged) {
                handleModelReload(config);
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
    if (wThread) {
        SDL_WaitThread(wThread, NULL);
    }
    whisperFree();
    cleanupAudio();
    destroyTray();
    if (textMutex) {
        SDL_DestroyMutex(textMutex);
    }
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
                    SDL_LockMutex(textMutex);
                    subtitleText[0] = '\0';
                    tokenNum = 0;
                    SDL_UnlockMutex(textMutex);
                    clearSubtitleText();
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
    while (!done) {
        if (chunkReady && !paused) {
            SDL_LockMutex(textMutex);
            subtitleText[0] = '\0';
            whisperProcess(audioChunk, SAMPLE_SIZE, subtitleText, sizeof(subtitleText), config->cpu_threads, config->language, outputTokens,
                           &tokenNum);
            textUpdated = true;
            SDL_UnlockMutex(textMutex);
            chunkReady = false;

            // Wake up the main event loop immediately
            SDL_Event event;
            SDL_zero(event);
            event.type = SDL_EVENT_USER;
            event.user.code = APP_EVENT_TEXT_UPDATED;
            SDL_PushEvent(&event);
        } else if (chunkReady && paused) {
            chunkReady = false; // discard stale audio collected while paused
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
