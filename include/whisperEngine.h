#pragma once
#include <stdbool.h>
#include <stddef.h>

bool whisperInit(const char *modelPath, bool *use_gpu);
bool whisperProcess(float *pcmf32, int n_samples, char *outputText, size_t outputLength, int n_threads);
void whisperFree(void);
