#include <stdio.h>
#include <stdlib.h>
#include "trayManager.h"
#include <SDL3/SDL.h>

static SDL_Tray *s_tray = NULL;
static SDL_Window *s_window = NULL;
static SDL_TrayEntry *s_focusEntry = NULL;
static SDL_TrayEntry *s_pauseEntry = NULL;
static bool s_paused = false;

// Forward declarations
static void buildMenu(void);
static void cb_focus(void *userdata, SDL_TrayEntry *entry);
static void cb_toggle(void *userdata, SDL_TrayEntry *entry);
static void cb_quit(void *userdata, SDL_TrayEntry *entry);

bool initTray(SDL_Window *window)
{
    s_window = window;

    // 1. Build an absolute path to guarantee we find the file
    char iconPath[512];
    const char* basePath = SDL_GetBasePath();
    snprintf(iconPath, sizeof(iconPath), "%sspaceholder_rts_icon.png", basePath);
    SDL_free((void*)basePath);

    SDL_Surface* icon = SDL_LoadPNG(iconPath);
    SDL_Log("Icon load: %s", icon ? "OK" : SDL_GetError());

    s_tray = SDL_CreateTray(icon, "Real-Time Subtitler");
    if (icon) SDL_DestroySurface(icon);

    if (!s_tray) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create tray: %s", SDL_GetError());
        return false;
    }

    buildMenu();

    return true;
}

void destroyTray(void)
{
    if (s_tray) {
        SDL_DestroyTray(s_tray);
        s_tray = NULL;
    }
}

// Builds the tray menu from scratch so Windows picks up any label changes.
static void buildMenu(void)
{
    if (!s_tray) return;

    SDL_TrayMenu *menu = SDL_GetTrayMenu(s_tray);
    if (menu) {
        // Clear existing entries so we don't duplicate on rebuild
        int count = 0;
        const SDL_TrayEntry **entries = SDL_GetTrayEntries(menu, &count);
        for (int i = count - 1; i >= 0; i--)
            SDL_RemoveTrayEntry((SDL_TrayEntry *)entries[i]);
    } else {
        // First call — no menu exists yet, create one
        menu = SDL_CreateTrayMenu(s_tray);
    }

    SDL_TrayEntry *title = SDL_InsertTrayEntryAt(menu, -1,
                               "Real-Time Subtitler", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryEnabled(title, false);

    SDL_InsertTrayEntryAt(menu, -1, NULL, 0);

    SDL_TrayEntry *focus = SDL_InsertTrayEntryAt(menu, -1,
                               "Focus Window", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(focus, cb_focus, NULL);

    s_pauseEntry = SDL_InsertTrayEntryAt(menu, -1,
                       s_paused ? "Resume" : "Pause", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(s_pauseEntry, cb_toggle, NULL);

    SDL_InsertTrayEntryAt(menu, -1, NULL, 0);

    SDL_TrayEntry *quit = SDL_InsertTrayEntryAt(menu, -1,
                              "Quit", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(quit, cb_quit, NULL);
}

// --- Callbacks (called on the main thread via SDL's event pump) ---

static void cb_focus(void *userdata, SDL_TrayEntry *entry)
{
    (void)userdata; (void)entry;
    
    SDL_SetWindowMousePassthrough(s_window, false);
    SDL_SetWindowBordered(s_window, true);
}

static void cb_toggle(void *userdata, SDL_TrayEntry *entry)
{
    (void)userdata; (void)entry;
    s_paused = !s_paused;

    // SDL3 on Windows caches the native menu; rebuilding it forces the OS
    // to re-read the entries so the new label is visible next open.
    buildMenu();

    // Push a custom user event so main.c can react (pause/resume audio)
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_USER;
    e.user.code = s_paused ? 1 : 0; // 1 = pause, 0 = resume
    SDL_PushEvent(&e);
}

static void cb_quit(void *userdata, SDL_TrayEntry *entry)
{
    (void)userdata; (void)entry;
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}