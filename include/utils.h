#pragma once
#include <stddef.h>
#include <stdbool.h>

/**
 * Resolves a relative path to a full path next to the application executable.
 *
 * @param dest The buffer to write the resolved path into.
 * @param destSize The capacity of the dest buffer.
 * @return true if resolved without truncation, false otherwise.
 */
bool utilsResolvePath(char *dest, size_t destSize, const char *relativePath);

/**
 * Checks if a file exists and is readable.
 *
 * @param relativePath The path relative to the application base folder.
 * @return true if the file exists and can be opened in binary read mode, false otherwise.
 */
bool utilsIsFileReadable(const char *relativePath);

#include <SDL3/SDL.h>

void RTS_LockMutex(SDL_Mutex *mutex);
void RTS_UnlockMutex(SDL_Mutex *mutex);

#ifdef RTS_MONKEY_TEST
#include <stdbool.h>

void utilsParseMonkeyArgs(int argc, char *argv[]);
bool utilsIsMonkeyModeEnabled(void);
int utilsRunMonkeyEventLoop(void *data);
void utilsMonkeyDelay(void);

#define MONKEY_DELAY() utilsMonkeyDelay()
#else
#define MONKEY_DELAY() ((void)0)
#endif
