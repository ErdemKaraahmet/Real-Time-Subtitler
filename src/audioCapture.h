#pragma once

#include <stdbool.h>
#include <stdlib.h>

bool initAudio();

// Start loopback capture device.
void startAudio();

// Stops the capture device.
void stopAudio();

/*
Fetches available 16kHz float samples from the internal buffer.
@param outBuffer Target array to store samples.
@param sampleSize used in Whisper also
 */
void getAudioChunk(float* outputBuffer, int sampleSize);

// Uninitializes the device and frees allocated memory
void cleanupAudio();