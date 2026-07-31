#include "whisperEngine.h"
#include "utils.h"
#include "whisper.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef GGML_USE_VULKAN
#include "ggml-vulkan.h"
#endif

bool whisperHasGpu(void) {
#ifdef GGML_USE_VULKAN
    return (ggml_backend_vk_get_device_count() > 0);
#else
    return false;
#endif
}

static struct whisper_context *ctx = NULL;

#ifdef RTS_BENCH
static FILE *benchFile = NULL;
#endif

// Initialize
bool whisperInit(const char *modelPath, bool *use_gpu) {
    char fullPath[512];
    utilsResolvePath(fullPath, sizeof(fullPath), modelPath);
    if (!utilsIsFileReadable(modelPath)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Model file is unreadable or missing: %s", fullPath);
        return false;
    }

    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.flash_attn = true; // reduces memory/latency

    // Try GPU if requested
    if (use_gpu && *use_gpu) {
        SDL_Log("Attempting GPU-accelerated whisper init...");
        cparams.use_gpu = true;
        ctx = whisper_init_from_file_with_params(fullPath, cparams);

        if (ctx == NULL) {
            // GPU failed, fall back to CPU
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GPU init failed, falling back to CPU...");
            cparams.use_gpu = false;
            *use_gpu = false; // report fallback back to config
            ctx = whisper_init_from_file_with_params(fullPath, cparams);
        }
    } else {
        SDL_Log("Attempting CPU whisper init...");
        cparams.use_gpu = false;
        ctx = whisper_init_from_file_with_params(fullPath, cparams);
    }

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
bool whisperProcess(float *pcmf32, int n_samples, char *outputText, size_t outputLength, int n_threads, SubtitleToken *outputTokens,
                    int *outputTokenNums) {
    if (!ctx)
        return false;

    struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress = false;
    wparams.language = "en";
    wparams.n_threads = n_threads > 0 ? n_threads : 1;
    wparams.no_timestamps = true;   // Reduces decode overhead
    wparams.single_segment = true;  // Force single segment
    wparams.no_context = true;      // Prevent using past chunks as context to avoid repetition loops
    wparams.temperature_inc = 0.5f; // Amount the temperature increases each time it retries, 1 is max so 0.5 is two retries
    wparams.audio_ctx = 384;        // Crop audio context window to size of 2s chunks plus safety padding, whisper is
                                    // trained on 30s chunks which is 1500 frames
    wparams.max_tokens = 32;        // Cap maximum tokens generated per chunk to stop hallucinations

#ifdef RTS_BENCH
    Uint64 t0 = SDL_GetPerformanceCounter();
#endif

    if (whisper_full(ctx, wparams, pcmf32, n_samples) != 0) {
        return false;
    }

    if (outputTokenNums) {
        *outputTokenNums = 0;
    }

    const int n_segments = whisper_full_n_segments(ctx);
    int token_count = 0;
    const whisper_token eot_token = whisper_token_eot(ctx);

    for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(ctx, i);
        if (text && strlen(text) > 0 && strstr(text, "[BLANK_AUDIO]") == NULL) {
            SDL_strlcat(outputText, text, outputLength);

            char tokenLogBuffer[4096] = {0};
            size_t logLen = 0;

            const int n_tokens = whisper_full_n_tokens(ctx, i);
            for (int j = 0; j < n_tokens; ++j) {
                const whisper_token token_id = whisper_full_get_token_id(ctx, i, j);
                if (token_id >= eot_token) {
                    break;
                }

                const char *tokenText = whisper_full_get_token_text(ctx, i, j);
                if (tokenText == NULL || tokenText[0] == '\0' || strcmp(tokenText, "<|endoftext|>") == 0 || strcmp(tokenText, "[_EOT_]") == 0) {
                    break;
                }
                if (strstr(tokenText, "[BLANK_AUDIO]") != NULL) {
                    continue;
                }

                float tokenProbability = whisper_full_get_token_data(ctx, i, j).p;

                if (outputTokens && token_count < 1024) {
                    SDL_strlcpy(outputTokens[token_count].text, tokenText, sizeof(outputTokens[token_count].text));
                    outputTokens[token_count].probability = tokenProbability;
                    token_count++;
                }

                const char *colorCode = "\033[32m"; // Green (high conf >= 0.8)
                if (tokenProbability < 0.50f) {
                    colorCode = "\033[31m"; // Red (low conf < 0.5)
                } else if (tokenProbability < 0.80f) {
                    colorCode = "\033[33m"; // Yellow (med conf 0.5..0.8)
                }

                int written = snprintf(tokenLogBuffer + logLen, sizeof(tokenLogBuffer) - logLen, "%s%s", colorCode, tokenText);
                if (written > 0 && logLen + (size_t)written < sizeof(tokenLogBuffer)) {
                    logLen += (size_t)written;
                }
            }

            if (logLen > 0) {
                SDL_strlcat(tokenLogBuffer, "\033[0m", sizeof(tokenLogBuffer));
                SDL_Log("%s", tokenLogBuffer);
            }
        }
    }

    if (outputTokenNums) {
        *outputTokenNums = token_count;
    }

#ifdef RTS_BENCH
    if (benchFile && n_segments > 0) {
        double inference_ms = (double)(SDL_GetPerformanceCounter() - t0) / (double)SDL_GetPerformanceFrequency() * 1000.0;
        float prob_sum = 0.0f;
        int token_count_bench = 0;
        for (int i = 0; i < n_segments; ++i) {
            int n_tok = whisper_full_n_tokens(ctx, i);
            for (int t = 0; t < n_tok; ++t) {
                // Skip special tokens (>= EOT)
                if (whisper_full_get_token_id(ctx, i, t) < eot_token) {
                    prob_sum += whisper_full_get_token_p(ctx, i, t);
                    token_count_bench++;
                }
            }
        }
        float avg_prob = token_count_bench > 0 ? prob_sum / (float)token_count_bench : 0.0f;
        fprintf(benchFile, "%.2f,%.4f,%d\n", inference_ms, avg_prob, token_count_bench);
        fflush(benchFile);
    }
#endif

    return true;
}

void whisperFree(void) {
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

// C interface
//
//  The following interface is thread-safe as long as the sample whisper_context is not used by multiple threads
//  concurrently.
//
//  Basic usage:
//
//      #include "whisper.h"
//
//      ...
//
//      whisper_context_params cparams = whisper_context_default_params();
//
//      struct whisper_context * ctx = whisper_init_from_file_with_params("/path/to/ggml-base.en.bin", cparams);
//
//      if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
//          fprintf(stderr, "failed to process audio\n");
//          return 7;
//      }
//
//      const int n_segments = whisper_full_n_segments(ctx);
//      for (int i = 0; i < n_segments; ++i) {
//          const char * text = whisper_full_get_segment_text(ctx, i);
//          printf("%s", text);
//      }
//
//      whisper_free(ctx);
//
//      ...
//
//  This is a demonstration of the most straightforward usage of the library.
//  "pcmf32" contains the RAW audio data in 32-bit floating point format.
//
//  The interface also allows for more fine-grained control over the computation, but it requires a deeper
//  understanding of how the model works.
