#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include "miniaudio.h"

bool initAndStartAudio(void);

// Stops the capture device.
void stopAudio(void);

/*
Fetches available 16kHz float samples from the internal buffer.
@param outBuffer Target array to store samples.
@param sampleSize used in Whisper also
 */
bool getAudioChunk(float *outputBuffer, int sampleSize);

// Checks if audio ring buffer has enough sample for whisper
bool audioChunkReady(unsigned int sampleSize);

// Uninitializes the device and frees allocated memory
void cleanupAudio(void);

// Pauses the audio capture device.
void pauseAudio(void);

// Resumes the audio capture device.
void resumeAudio(void);