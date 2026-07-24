#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audioCapture.h"
#include <stdio.h>
#include <string.h>

#define FORMAT ma_format_f32
#define CHANNELS 1
#define SAMPLE_RATE 16000 // 16Khz
#define SECONDS_IN_BUFFER 5
#define BUFFER_SIZE_IN_FRAMES (SAMPLE_RATE * SECONDS_IN_BUFFER)
#define RMS_THRESHOLD 0.001f

static ma_device device;
static ma_pcm_rb ringBuffer;
// single producer, single consumer so no lock needed

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    void *pRingBuffer;
    ma_pcm_rb_acquire_write(&ringBuffer, &frameCount, &pRingBuffer);
    memcpy(pRingBuffer, pInput, frameCount * sizeof(float));
    ma_pcm_rb_commit_write(&ringBuffer, frameCount);

    (void)pOutput;
}

bool initAndStartAudio(void) {
    ma_result result;
    ma_device_config deviceConfig;

    // init pcm ring buffer
    result = ma_pcm_rb_init(FORMAT, CHANNELS, BUFFER_SIZE_IN_FRAMES, NULL, NULL, &ringBuffer);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize ring buffer.\n");
        return false;
    }

#if defined(_WIN32)
    ma_backend backends[] = {ma_backend_wasapi};

    deviceConfig = ma_device_config_init(ma_device_type_loopback);
    deviceConfig.capture.pDeviceID = NULL;    // Use default device.
    deviceConfig.capture.format = FORMAT;     // 32 bit float
    deviceConfig.capture.channels = CHANNELS; // single channel
    deviceConfig.sampleRate = SAMPLE_RATE;    // 16Khz
    deviceConfig.dataCallback = data_callback;

    result = ma_device_init_ex(backends, sizeof(backends) / sizeof(backends[0]), NULL, &deviceConfig, &device);
#elif defined(__linux__) || defined(__APPLE__)
    ma_bool32 foundMonitor = MA_FALSE;
    ma_device_id monitorDeviceID;

#if defined(__linux__)
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) == MA_SUCCESS) {
        ma_device_info *pPlaybackInfos;
        ma_uint32 playbackCount;
        ma_device_info *pCaptureInfos;
        ma_uint32 captureCount;

        if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) == MA_SUCCESS) {
            for (ma_uint32 i = 0; i < captureCount; i++) {
                if (strstr(pCaptureInfos[i].name, "monitor") != NULL || strstr(pCaptureInfos[i].name, "Monitor") != NULL) {
                    monitorDeviceID = pCaptureInfos[i].id;
                    foundMonitor = MA_TRUE;
                    printf("Auto-detected system loopback/monitor device: %s\n", pCaptureInfos[i].name);
                    break;
                }
            }
        }
        ma_context_uninit(&context);
    }
#endif

    deviceConfig = ma_device_config_init(ma_device_type_capture);
    if (foundMonitor) {
        deviceConfig.capture.pDeviceID = &monitorDeviceID;
    } else {
        deviceConfig.capture.pDeviceID = NULL; // Use default device.
        printf("No system loopback/monitor device found. Falling back to default capture device.\n");
    }
    deviceConfig.capture.format = FORMAT;     // 32 bit float
    deviceConfig.capture.channels = CHANNELS; // single channel
    deviceConfig.sampleRate = SAMPLE_RATE;    // 16Khz
    deviceConfig.dataCallback = data_callback;

    result = ma_device_init(NULL, &deviceConfig, &device);
#else
#error "Audio Capture for this os is not implemented"
#endif

    if (result != MA_SUCCESS) {
        printf("Failed to initialize capture device.\n");
        return false;
    }

    // Start capture/loopback device.
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
bool getAudioChunk(float *outputBuffer, int sampleSize) {
    void *pRingBuffer;

    ma_pcm_rb_acquire_read(&ringBuffer, &sampleSize, &pRingBuffer);
    memcpy(outputBuffer, pRingBuffer, sampleSize * sizeof(float));
    ma_pcm_rb_commit_read(&ringBuffer, sampleSize);

    // Check for sound activity (VAD)
    float sum = 0.0f;
    for (int i = 0; i < sampleSize; ++i) {
        float val = outputBuffer[i];
        sum += (val < 0.0f) ? -val : val;
    }
    return (sum / sampleSize) > RMS_THRESHOLD;
}

bool audioChunkReady(ma_uint32 sampleSize) {
    return ma_pcm_rb_available_read(&ringBuffer) >= sampleSize;
}

// Uninitializes the device and frees allocated memory
void cleanupAudio(void) {
    ma_device_uninit(&device);
}

void pauseAudio(void) {
    ma_device_stop(&device);
}

void resumeAudio(void) {
    ma_device_start(&device);
}