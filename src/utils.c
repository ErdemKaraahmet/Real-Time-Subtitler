#include <SDL3/SDL.h>
#include <stdio.h>
#include "utils.h"

bool utilsResolvePath(char *dest, size_t destSize, const char *relativePath) {
    const char *basePath = SDL_GetBasePath();
    if (basePath) {
        int res = snprintf(dest, destSize, "%s%s", basePath, relativePath);
        if (res >= 0 && (size_t)res < destSize) {
            return true;
        }
    }
    SDL_strlcpy(dest, relativePath, destSize);
    return false;
}

bool utilsIsFileReadable(const char *relativePath) {
    char fullPath[512];
    utilsResolvePath(fullPath, sizeof(fullPath), relativePath);
    return SDL_GetPathInfo(fullPath, NULL);
}

void RTS_LockMutex(SDL_Mutex *mutex) {
    MONKEY_DELAY();
    SDL_LockMutex(mutex);
}

void RTS_UnlockMutex(SDL_Mutex *mutex) {
    SDL_UnlockMutex(mutex);
    MONKEY_DELAY();
}

#ifdef RTS_MONKEY_TEST
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "appEvents.h"

static bool g_monkeyModeEnabled = false;
static unsigned int g_monkeySeed = 0;

void utilsParseMonkeyArgs(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--monkey") == 0 || strcmp(argv[i], "-m") == 0) {
            unsigned int seed = 0;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                bool isNumeric = true;
                const char *argStr = argv[i + 1];
                if (argStr[0] == '\0') {
                    isNumeric = false;
                } else {
                    for (const char *p = argStr; *p != '\0'; p++) {
                        if (*p < '0' || *p > '9') {
                            isNumeric = false;
                            break;
                        }
                    }
                }
                if (isNumeric) {
                    seed = (unsigned int)strtoul(argStr, NULL, 10);
                    i++;
                } else {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[MONKEY] Non-numeric seed '%s' passed; falling back to auto-generated seed.", argStr);
                    seed = (unsigned int)SDL_GetPerformanceCounter();
                }
            } else {
                seed = (unsigned int)SDL_GetPerformanceCounter();
            }
            g_monkeyModeEnabled = true;
            g_monkeySeed = seed;
        }
    }
}

bool utilsIsMonkeyModeEnabled(void) {
    return g_monkeyModeEnabled;
}

int utilsRunMonkeyEventLoop(void *data) {
    const volatile bool *pDone = (volatile bool *)data;
    unsigned int monkeySeed = g_monkeySeed;
    SDL_Log("[MONKEY] Event monkey testing thread started with seed: %u", monkeySeed);
    unsigned int state = monkeySeed;
    Uint64 eventCount = 0;

    const AppUserEvent events[] = {APP_EVENT_PAUSE, APP_EVENT_RESUME, APP_EVENT_MOVE_WINDOW, APP_EVENT_OPEN_CONTROL};
    const int numEvents = (int)(sizeof(events) / sizeof(events[0]));

    while (pDone && !*pDone) {
        // LCG pseudo-random generator
        state = state * 1103515245U + 12345U;
        int idx = (int)((state / 65536U) % (unsigned int)numEvents);

        SDL_Event event;
        SDL_zero(event);
        event.type = SDL_EVENT_USER;
        event.user.code = (Sint32)events[idx];
        SDL_PushEvent(&event);

        eventCount++;
        if (eventCount % 50 == 0) {
            SDL_Log("[MONKEY] Pushed %" PRIu64 " monkey events (latest code: %d)", eventCount, (int)events[idx]);
        }

        state = state * 1103515245U + 12345U;
        Uint32 delayMs = 5 + ((state / 65536U) % 16U); // 5ms - 20ms delay
        SDL_Delay(delayMs);
    }
    SDL_Log("[MONKEY] Event monkey testing thread stopping (seed: %u). Total events pushed: %" PRIu64, monkeySeed, eventCount);
    return 0;
}

void utilsMonkeyDelay(void) {
    if (!g_monkeyModeEnabled) {
        return;
    }

    // Each thread gets its own private copy of this variable (_Thread_local),
    // so main/whisper/audio threads never touch the same PRNG state at once.
    // That avoids needing a mutex.
    static _Thread_local unsigned int lockPrngState;
    if (!lockPrngState) {
        lockPrngState = (g_monkeySeed ^ 0x9E3779B9U) | 1U;
    }
    lockPrngState = lockPrngState * 1103515245U + 12345U;
    Uint32 delayMs = 1 + ((lockPrngState / 65536U) % 5U); // 1ms - 5ms delay
    SDL_Delay(delayMs);
}
#endif
