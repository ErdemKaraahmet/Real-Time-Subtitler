#pragma once
#include <stdbool.h>

bool whisperInit(const char* modelPath);
bool whisperProcess(float* pcmf32, int n_samples, char* outputText, int outputLength);
void whisperFree();