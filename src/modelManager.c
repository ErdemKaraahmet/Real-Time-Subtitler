#include "modelManager.h"
#include "cJSON.h"
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static ModelManager g_ModelManager;
static SDL_Thread* g_FetchThread = NULL;
static SDL_AtomicInt g_FetchFinished;

// Helper to get name by stripping "ggml-" and ".bin"
static void getDisplayName(const char* filename, char* dest, size_t destSize) {
    const char* start = filename;
    if (strncmp(filename, "ggml-", 5) == 0) {
        start += 5;
    }
    size_t len = strlen(start);
    if (len > 4 && strcmp(start + len - 4, ".bin") == 0) {
        len -= 4;
    }
    if (len >= destSize) {
        len = destSize - 1;
    }
    memcpy(dest, start, len);
    dest[len] = '\0';
}

ModelManager* getModelManager(void) {
    return &g_ModelManager;
}

// Enumerate local model files callback
static SDL_EnumerationResult SDLCALL scanLocalModelsCallback(void *userdata, const char *dirname, const char *fname) {
    (void)userdata; (void)dirname;
    size_t len = strlen(fname);
    
    // Check for "ggml-*.bin" pattern
    if (len > 4 && strncmp(fname, "ggml-", 5) == 0 && SDL_strcasecmp(fname + len - 4, ".bin") == 0) {
        SDL_LockMutex(g_ModelManager.lock);
        
        // Avoid duplicate entry if scanned again
        bool exists = false;
        for (int i = 0; i < g_ModelManager.count; ++i) {
            if (strcmp(g_ModelManager.models[i].filename, fname) == 0) {
                g_ModelManager.models[i].state = MODEL_STATE_DOWNLOADED;
                exists = true;
                break;
            }
        }
        
        if (!exists && g_ModelManager.count < MODEL_MAX_COUNT) {
            ModelEntry* entry = &g_ModelManager.models[g_ModelManager.count];
            SDL_strlcpy(entry->filename, fname, sizeof(entry->filename));
            getDisplayName(fname, entry->name, sizeof(entry->name));
            
            // Get local file size
            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", dirname, fname);
            SDL_PathInfo info;
            entry->remoteSize = SDL_GetPathInfo(fullPath, &info) ? info.size : 0;
            
            entry->state = MODEL_STATE_DOWNLOADED;
            entry->oid[0] = '\0';
            SDL_SetAtomicInt(&entry->progressPercent, 100);
            entry->errorMessage[0] = '\0';
            
            g_ModelManager.count++;
        }
        
        SDL_UnlockMutex(g_ModelManager.lock);
    }
    return SDL_ENUM_CONTINUE;
}

static void scanLocalModels(void) {
    char path[512];
    const char* basePath = SDL_GetBasePath();
    snprintf(path, sizeof(path), "%smodels", basePath);
    SDL_EnumerateDirectory(path, scanLocalModelsCallback, NULL);
}

void modelManagerInit(void) {
    memset(&g_ModelManager, 0, sizeof(g_ModelManager));
    g_ModelManager.lock = SDL_CreateMutex();
    SDL_SetAtomicInt(&g_FetchFinished, 0);
    
    // Scan already existing models on startup
    scanLocalModels();
}

void modelManagerShutdown(void) {
    if (g_FetchThread) {
        int status;
        SDL_WaitThread(g_FetchThread, &status);
        g_FetchThread = NULL;
    }
    
    if (g_ModelManager.lock) {
        SDL_DestroyMutex(g_ModelManager.lock);
        g_ModelManager.lock = NULL;
    }
}

// Curl write callback to accumulate JSON payload
typedef struct {
    char* memory;
    size_t size;
} MemoryBuffer;

static size_t writeMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    MemoryBuffer* mem = (MemoryBuffer*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        return 0; // out of memory
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Background thread function to fetch tree catalog from HF API
static int SDLCALL fetchCatalogThreadFunc(void* data) {
    (void)data;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        SDL_SetAtomicInt(&g_FetchFinished, 1);
        return -1;
    }
    
    MemoryBuffer chunk = {0};
    
    // API endpoint for whisper.cpp model repository
    curl_easy_setopt(curl, CURLOPT_URL, "https://huggingface.co/api/models/ggerganov/whisper.cpp/tree/main");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Real-Time-Subtitler/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L); // 15 seconds timeout
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);  // Thread-safe signaling
    
    CURLcode res = curl_easy_perform(curl);
    bool success = false;
    
    if (res == CURLE_OK) {
        cJSON* root = cJSON_Parse(chunk.memory);
        if (root && cJSON_IsArray(root)) {
            success = true;
            SDL_LockMutex(g_ModelManager.lock);
            
            cJSON* element = NULL;
            cJSON_ArrayForEach(element, root) {
                cJSON* typeItem = cJSON_GetObjectItemCaseSensitive(element, "type");
                cJSON* pathItem = cJSON_GetObjectItemCaseSensitive(element, "path");
                
                if (cJSON_IsString(typeItem) && strcmp(typeItem->valuestring, "file") == 0 &&
                    cJSON_IsString(pathItem)) {
                    
                    const char* path = pathItem->valuestring;
                    size_t pathLen = strlen(path);
                    
                    // Filter: must start with "ggml-" and end in ".bin"
                    if (pathLen > 4 && strncmp(path, "ggml-", 5) == 0 && strcmp(path + pathLen - 4, ".bin") == 0) {
                        cJSON* sizeItem = cJSON_GetObjectItemCaseSensitive(element, "size");
                        cJSON* lfsItem = cJSON_GetObjectItemCaseSensitive(element, "lfs");
                        int64_t remoteSize = sizeItem ? (int64_t)sizeItem->valuedouble : 0;
                        const char* oid = "";
                        
                        if (cJSON_IsObject(lfsItem)) {
                            cJSON* oidItem = cJSON_GetObjectItemCaseSensitive(lfsItem, "oid");
                            if (cJSON_IsString(oidItem)) {
                                oid = oidItem->valuestring;
                                if (strncmp(oid, "sha256:", 7) == 0) {
                                    oid += 7; // strip standard HF Git-LFS prefix
                                }
                            }
                        }
                        
                        // Merge with scanned files or add new one
                        bool found = false;
                        for (int i = 0; i < g_ModelManager.count; ++i) {
                            if (strcmp(g_ModelManager.models[i].filename, path) == 0) {
                                g_ModelManager.models[i].remoteSize = remoteSize;
                                SDL_strlcpy(g_ModelManager.models[i].oid, oid, sizeof(g_ModelManager.models[i].oid));
                                found = true;
                                break;
                            }
                        }
                        
                        if (!found && g_ModelManager.count < MODEL_MAX_COUNT) {
                            ModelEntry* entry = &g_ModelManager.models[g_ModelManager.count];
                            SDL_strlcpy(entry->filename, path, sizeof(entry->filename));
                            getDisplayName(path, entry->name, sizeof(entry->name));
                            entry->remoteSize = remoteSize;
                            SDL_strlcpy(entry->oid, oid, sizeof(entry->oid));
                            entry->state = MODEL_STATE_NOT_DOWNLOADED;
                            SDL_SetAtomicInt(&entry->progressPercent, 0);
                            entry->errorMessage[0] = '\0';
                            
                            g_ModelManager.count++;
                        }
                    }
                }
            }
            SDL_UnlockMutex(g_ModelManager.lock);
            cJSON_Delete(root);
        }
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to fetch HF catalog tree: %s", curl_easy_strerror(res));
    }
    
    free(chunk.memory);
    curl_easy_cleanup(curl);
    
    SDL_LockMutex(g_ModelManager.lock);
    g_ModelManager.catalogFetched = success;
    SDL_UnlockMutex(g_ModelManager.lock);
    
    SDL_SetAtomicInt(&g_FetchFinished, 1);
    return 0;
}

void modelManagerStartFetchCatalog(void) {
    if (g_ModelManager.fetchInProgress) return;
    
    g_ModelManager.fetchInProgress = true;
    SDL_SetAtomicInt(&g_FetchFinished, 0);
    
    g_FetchThread = SDL_CreateThread(fetchCatalogThreadFunc, "CatalogFetch", NULL);
    if (!g_FetchThread) {
        g_ModelManager.fetchInProgress = false;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create catalog fetch thread: %s", SDL_GetError());
    }
}

void modelManagerPoll(void) {
    if (g_ModelManager.fetchInProgress && SDL_GetAtomicInt(&g_FetchFinished)) {
        int status;
        SDL_WaitThread(g_FetchThread, &status);
        g_FetchThread = NULL;
        
        g_ModelManager.fetchInProgress = false;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Model catalog fetch thread joined (status: %d)", status);
    }
}

bool modelManagerStartDownload(int index) {
    (void)index;
    return false;
}

void modelManagerCancelDownload(void) {
}

bool modelManagerDeleteModel(int index) {
    (void)index;
    return false;
}

bool modelManagerIsDownloading(void) {
    return false;
}
