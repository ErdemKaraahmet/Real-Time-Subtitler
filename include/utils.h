#pragma once
#include <stddef.h>
#include <stdbool.h>

/**
 * Resolves a relative path to a full path next to the application executable.
 *
 * @param dest The buffer to write the resolved path into.
 * @param destSize The capacity of the dest buffer.
 * @param relativePath The path relative to the application base folder.
 */
void utilsResolvePath(char* dest, size_t destSize, const char* relativePath);

/**
 * Checks if a file exists and is readable.
 *
 * @param relativePath The path relative to the application base folder.
 * @return true if the file exists and can be opened in binary read mode, false otherwise.
 */
bool utilsIsFileReadable(const char* relativePath);
