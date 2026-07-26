#pragma once
#include <stdbool.h>

typedef struct
{
    char text[64];
    float probability;
} SubtitleToken;

bool whisperInit(const char *modelPath, bool *use_gpu);
bool whisperProcess(float *pcmf32, int n_samples, char *outputText, int outputLength, SubtitleToken* outputTokens, int *ontputTokenNums);
void whisperFree(void);
