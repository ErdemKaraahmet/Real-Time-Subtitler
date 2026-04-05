#include "whisperEngine.h"
#include "whisper.h"
#include <stdio.h>
#include <string.h>

static struct whisper_context* ctx = NULL;

// Initialize
bool whisperInit(const char* modelPath) {

}

// Returns true if there is new text
bool whisperProcess(float* pcmf32, int n_samples, char* outputText, int outputLength) {

}

void whisperFree() {

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