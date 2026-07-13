#include "whisperEngine.h"
#include "whisper.h"
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

static struct whisper_context* ctx = NULL;

#ifdef RTS_BENCH
static FILE* benchFile = NULL;
#endif

void quiet_log_callback(enum ggml_log_level level, const char * text, void * user_data) {
    // Leave this completely empty to discard all logs
}

// Initialize
bool whisperInit(const char* modelPath, bool* use_gpu) {
    struct whisper_context_params cparams = whisper_context_default_params();

    // Try GPU if requested
    if (use_gpu && *use_gpu) {
        SDL_Log("Attempting GPU-accelerated whisper init...");
        cparams.use_gpu = true;
        ctx = whisper_init_from_file_with_params(modelPath, cparams);

        if (ctx == NULL) {
            // GPU failed, fall back to CPU
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GPU init failed, falling back to CPU...");
            cparams.use_gpu = false;
            *use_gpu = false; // report fallback back to config
            ctx = whisper_init_from_file_with_params(modelPath, cparams);
        }
    } else {
        SDL_Log("Attempting CPU whisper init...");
        cparams.use_gpu = false;
        ctx = whisper_init_from_file_with_params(modelPath, cparams);
    }

    // Now silence whisper's internal logging for runtime
    whisper_log_set(quiet_log_callback, NULL);

    if (ctx != NULL) {
        SDL_Log("Whisper context created successfully (GPU: %s)", cparams.use_gpu ? "yes" : "no");
#ifdef RTS_BENCH
        benchFile = fopen("bench/rts_bench.csv", "w");
        if (benchFile) {
            fprintf(benchFile, "model,%s\n", modelPath);
            fprintf(benchFile, "inference_ms,avg_token_prob,n_tokens\n");
            fflush(benchFile);
        }
#endif
    }
    return (ctx != NULL);
}

// Returns true if there is new text
bool whisperProcess(float* pcmf32, int n_samples, char* outputText, int outputLength) {
    if (!ctx) return false;

    struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress = false;
    wparams.language = "en";
    wparams.n_threads = SDL_GetNumLogicalCPUCores(); 

#ifdef RTS_BENCH
    Uint64 t0 = SDL_GetPerformanceCounter();
#endif

    if (whisper_full(ctx, wparams, pcmf32, n_samples) != 0) {
        return false;
    }

    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char * text = whisper_full_get_segment_text(ctx, i);
        strncat(outputText, text, outputLength);
        if (text && strlen(text) > 0) {
            printf("%s", text);
        }
    }

#ifdef RTS_BENCH
    if (benchFile && n_segments > 0) {
        double inference_ms = (double)(SDL_GetPerformanceCounter() - t0) / SDL_GetPerformanceFrequency() * 1000.0;
        float prob_sum = 0.0f;
        int token_count = 0;
        for (int i = 0; i < n_segments; ++i) {
            int n_tok = whisper_full_n_tokens(ctx, i);
            for (int t = 0; t < n_tok; ++t) {
                // Skip special tokens (>= EOT)
                if (whisper_full_get_token_id(ctx, i, t) < whisper_token_eot(ctx)) {
                    prob_sum += whisper_full_get_token_p(ctx, i, t);
                    token_count++;
                }
            }
        }
        float avg_prob = token_count > 0 ? prob_sum / token_count : 0.0f;
        fprintf(benchFile, "%.2f,%.4f,%d\n",
            inference_ms, avg_prob, token_count);
        fflush(benchFile);
    }
#endif

    return true;
}

void whisperFree() {
#ifdef RTS_BENCH
    if (benchFile) {
        fclose(benchFile);
        benchFile = NULL;
    }
#endif
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