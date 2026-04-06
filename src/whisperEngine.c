#include "whisperEngine.h"
#include "whisper.h"
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

static struct whisper_context* ctx = NULL;

// Initialize
bool whisperInit(const char* modelPath) {
    struct whisper_context_params cparams = whisper_context_default_params();
    //cparams.use_gpu = true;
    ctx = whisper_init_from_file_with_params(modelPath, cparams);
    return (ctx != NULL);
}

// Returns true if there is new text
bool whisperProcess(float* pcmf32, int n_samples, char* outputText, int outputLength) {
    if (!ctx) return false;

    struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress = false;
    wparams.language = "en";
    wparams.n_threads = SDL_GetNumLogicalCPUCores(); 

    if (whisper_full(ctx, wparams, pcmf32, n_samples) != 0) {
        return false;
    }

    const int n_segments = whisper_full_n_segments(ctx);
    printf("segments: %d\n", n_segments);  // add this
    for (int i = 0; i < n_segments; ++i) {
        const char * text = whisper_full_get_segment_text(ctx, i);
        strncat(outputText, text, outputLength);
    }

    return true;
}

void whisperFree() {
    if (ctx != NULL) {
        whisper_free(ctx);
        ctx = NULL;
    }
}

// Example from whisper github repo

//C interface
//
// The following interface is thread-safe as long as the sample whisper_context is not used by multiple threads
// concurrently.
//
// Basic usage:
//
//     #include "whisper.h"
//
//     ...
//
//     whisper_context_params cparams = whisper_context_default_params();
//
//     struct whisper_context * ctx = whisper_init_from_file_with_params("/path/to/ggml-base.en.bin", cparams);
//
//     if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
//         fprintf(stderr, "failed to process audio\n");
//         return 7;
//     }
//
//     const int n_segments = whisper_full_n_segments(ctx);
//     for (int i = 0; i < n_segments; ++i) {
//         const char * text = whisper_full_get_segment_text(ctx, i);
//         printf("%s", text);
//     }
//
//     whisper_free(ctx);
//
//     ...
//
// This is a demonstration of the most straightforward usage of the library.
// "pcmf32" contains the RAW audio data in 32-bit floating point format.
//
// The interface also allows for more fine-grained control over the computation, but it requires a deeper
// understanding of how the model works.