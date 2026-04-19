#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audioCapture.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
// Windows loopback implementation using miniAudio

    #define FORMAT ma_format_f32
    #define CHANNELS 1
    #define SAMPLE_RATE 16000 // 16Khz 
    #define SECONDS_IN_BUFFER 5
    #define BUFFER_SIZE_IN_FRAMES (SAMPLE_RATE * SECONDS_IN_BUFFER)

    static ma_device device;
    static ma_pcm_rb ringBuffer;
    // single producer, single consumer so no lock needed

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    void *pRingBuffer;
    ma_pcm_rb_acquire_write(&ringBuffer, &frameCount, &pRingBuffer);
    memcpy(pRingBuffer, pInput, frameCount * sizeof(float));
    ma_pcm_rb_commit_write(&ringBuffer, frameCount);

    (void)pOutput;
}

bool initAndStartAudio()
{
    ma_result result;
    ma_device_config deviceConfig; 

    // init pcm ring buffer
    result = ma_pcm_rb_init(FORMAT, CHANNELS, BUFFER_SIZE_IN_FRAMES, NULL, NULL, &ringBuffer);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize ring buffer.\n");
        return false;
    }

    ma_backend backends[] = {
        ma_backend_wasapi
    };

    deviceConfig = ma_device_config_init(ma_device_type_loopback);
    deviceConfig.capture.pDeviceID = NULL; // Use default device.
    deviceConfig.capture.format    = FORMAT; // 32 bit float
    deviceConfig.capture.channels  = CHANNELS; // single channel
    deviceConfig.sampleRate        = SAMPLE_RATE; // 16Khz
    deviceConfig.dataCallback      = data_callback;

    result = ma_device_init_ex(backends, sizeof(backends)/sizeof(backends[0]), NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize loopback device.\n");
        return false;
    }

    // Start loopback capture device.
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        printf("Failed to start device.\n");
        return false;
    }
    return true;
}
/*
Fetches available 16kHz float samples from the internal buffer.
@param outBuffer Target array to store samples.
@param sampleSize used in Whisper also
 */
void getAudioChunk(float* outputBuffer, int sampleSize)
{
    void *pRingBuffer;
    
    ma_pcm_rb_acquire_read(&ringBuffer, &sampleSize, &pRingBuffer);
    memcpy(outputBuffer, pRingBuffer, sampleSize * sizeof(float));
    ma_pcm_rb_commit_read(&ringBuffer, sampleSize);
    printf("got %d frames\n", sampleSize);  // add this

}

bool audioChunkReady(ma_uint32 sampleSize) {
    return ma_pcm_rb_available_read(&ringBuffer) >= sampleSize;
}

// Uninitializes the device and frees allocated memory
void cleanupAudio()
{
    ma_device_uninit(&device);
}

void pauseAudio()
{
    ma_device_stop(&device);
}

void resumeAudio()
{
    ma_device_start(&device);
}

#elif defined(__linux__)

#elif defined(__APPLE__)

#else
    #error "Audio Capture for this os is not implemented"
#endif