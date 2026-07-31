#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char text[64];
    float probability;
} SubtitleToken;

bool whisperInit(const char *modelPath, bool *use_gpu);
bool whisperProcess(float *pcmf32, int n_samples, char *outputText, size_t outputLength, int n_threads, SubtitleToken *outputTokens,
                    int *outputTokenNums);
void whisperFree(void);
