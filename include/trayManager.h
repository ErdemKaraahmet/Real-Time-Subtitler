#pragma once
#include <SDL3/SDL.h>
#include <stdbool.h>

bool initTray(void);
void destroyTray(void);
void setTrayPauseState(bool paused);
