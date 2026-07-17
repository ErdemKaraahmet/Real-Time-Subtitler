#include "modelManager.h"
#include "cJSON.h"
#include "sha256.h"
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static ModelManager g_ModelManager;
static SDL_Thread* g_FetchThread = NULL;
static SDL_AtomicInt g_FetchFinished;

static SDL_Thread* g_DownloadThread = NULL;
static SDL_AtomicInt g_DownloadFinished;
static SDL_AtomicInt g_DownloadCancelFlag;
static int g_ActiveDownloadIndex = -1;

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
                // Protect active downloading and verifying states from being overwritten
                if (g_ModelManager.models[i].state != MODEL_STATE_DOWNLOADING &&
                    g_ModelManager.models[i].state != MODEL_STATE_VERIFYING) {
                    g_ModelManager.models[i].state = MODEL_STATE_DOWNLOADED;
                }
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
    SDL_SetAtomicInt(&g_DownloadFinished, 0);
    SDL_SetAtomicInt(&g_DownloadCancelFlag, 0);
    g_ActiveDownloadIndex = -1;
    
    // Scan already existing models on startup
    scanLocalModels();
}

void modelManagerShutdown(void) {
    if (g_ActiveDownloadIndex != -1) {
        modelManagerCancelDownload();
        if (g_DownloadThread) {
            int status;
            SDL_WaitThread(g_DownloadThread, &status);
            g_DownloadThread = NULL;
        }
    }

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

typedef struct {
    int index;
    int64_t resumeOffset;
} DownloadProgressData;

static int downloadProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal; (void)ulnow;
    DownloadProgressData* data = (DownloadProgressData*)clientp;
    
    if (SDL_GetAtomicInt(&g_DownloadCancelFlag) == 1) {
        return 1; // Abort transfer
    }
    
    SDL_LockMutex(g_ModelManager.lock);
    ModelEntry* entry = &g_ModelManager.models[data->index];
    int64_t remoteSize = entry->remoteSize;
    SDL_UnlockMutex(g_ModelManager.lock);
    
    int64_t total = data->resumeOffset + dltotal;
    int64_t now = data->resumeOffset + dlnow;
    
    if (total <= 0 && remoteSize > 0) {
        total = remoteSize;
    }
    
    int pct = (total > 0) ? (int)(now * 100 / total) : 0;
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    
    SDL_SetAtomicInt(&entry->progressPercent, pct);
    return 0;
}

static bool calculateFileSHA256(const char* filePath, char* destHex) {
    FILE* file = fopen(filePath, "rb");
    if (!file) return false;
    
    SHA256_CTX ctx;
    sha256_init(&ctx);
    
    uint8_t buffer[65536];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        sha256_update(&ctx, (const SHA256_BYTE*)buffer, bytesRead);
    }
    
    fclose(file);
    
    SHA256_BYTE hash[SHA256_BLOCK_SIZE];
    sha256_final(&ctx, hash);
    
    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        sprintf(destHex + (i * 2), "%02x", hash[i]);
    }
    destHex[64] = '\0';
    return true;
}

static int SDLCALL downloadThreadFunc(void* data) {
    int index = (int)(uintptr_t)data;
    
    SDL_LockMutex(g_ModelManager.lock);
    ModelEntry* entry = &g_ModelManager.models[index];
    char filename[MODEL_NAME_MAX];
    SDL_strlcpy(filename, entry->filename, sizeof(filename));
    SDL_UnlockMutex(g_ModelManager.lock);
    
    char basePath[512];
    SDL_strlcpy(basePath, SDL_GetBasePath(), sizeof(basePath));
    
    char partPath[512];
    snprintf(partPath, sizeof(partPath), "%smodels/%s.part", basePath, filename);
    
    // Check if partial file exists for resume support
    int64_t resumeOffset = 0;
    SDL_PathInfo pathInfo;
    if (SDL_GetPathInfo(partPath, &pathInfo)) {
        resumeOffset = pathInfo.size;
    }
    
    FILE* file = fopen(partPath, resumeOffset > 0 ? "ab" : "wb");
    if (!file) {
        SDL_LockMutex(g_ModelManager.lock);
        entry->state = MODEL_STATE_DOWNLOAD_ERROR;
        SDL_strlcpy(entry->errorMessage, "Failed to open local destination file", sizeof(entry->errorMessage));
        SDL_UnlockMutex(g_ModelManager.lock);
        
        SDL_SetAtomicInt(&g_DownloadFinished, 1);
        return -1;
    }
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(file);
        SDL_LockMutex(g_ModelManager.lock);
        entry->state = MODEL_STATE_DOWNLOAD_ERROR;
        SDL_strlcpy(entry->errorMessage, "Failed to initialize libcurl", sizeof(entry->errorMessage));
        SDL_UnlockMutex(g_ModelManager.lock);
        
        SDL_SetAtomicInt(&g_DownloadFinished, 1);
        return -1;
    }
    
    char url[512];
    snprintf(url, sizeof(url), "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/%s", filename);
    
    DownloadProgressData progressData = {
        .index = index,
        .resumeOffset = resumeOffset
    };
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, downloadProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, (void*)&progressData);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Real-Time-Subtitler/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L); // 10 seconds connection timeout
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    
    if (resumeOffset > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)resumeOffset);
    }
    
    CURLcode res = curl_easy_perform(curl);
    fclose(file);
    curl_easy_cleanup(curl);
    
    SDL_LockMutex(g_ModelManager.lock);
    bool cancelled = (SDL_GetAtomicInt(&g_DownloadCancelFlag) == 1);
    
    if (res == CURLE_OK && !cancelled) {
        entry->state = MODEL_STATE_VERIFYING;
        
        char expectedSha[65];
        SDL_strlcpy(expectedSha, entry->oid, sizeof(expectedSha));
        SDL_UnlockMutex(g_ModelManager.lock);
        
        // Run SHA-256 integrity check
        char computedSha[65] = {0};
        bool shaSuccess = calculateFileSHA256(partPath, computedSha);
        
        if (!shaSuccess) {
            SDL_LockMutex(g_ModelManager.lock);
            entry->state = MODEL_STATE_DOWNLOAD_ERROR;
            SDL_strlcpy(entry->errorMessage, "Failed to compute file integrity checksum", sizeof(entry->errorMessage));
            SDL_UnlockMutex(g_ModelManager.lock);
            SDL_RemovePath(partPath);
        } else if (expectedSha[0] != '\0' && strcmp(computedSha, expectedSha) != 0) {
            SDL_LockMutex(g_ModelManager.lock);
            entry->state = MODEL_STATE_DOWNLOAD_ERROR;
            snprintf(entry->errorMessage, sizeof(entry->errorMessage), "Integrity mismatch! Expected: %s, got: %s", expectedSha, computedSha);
            SDL_UnlockMutex(g_ModelManager.lock);
            SDL_RemovePath(partPath);
        } else {
            char binPath[512];
            snprintf(binPath, sizeof(binPath), "%smodels/%s", basePath, filename);
            SDL_RemovePath(binPath);
            
            if (SDL_RenamePath(partPath, binPath)) {
                SDL_LockMutex(g_ModelManager.lock);
                entry->state = MODEL_STATE_DOWNLOADED;
                SDL_SetAtomicInt(&entry->progressPercent, 100);
                SDL_UnlockMutex(g_ModelManager.lock);
            } else {
                SDL_LockMutex(g_ModelManager.lock);
                entry->state = MODEL_STATE_DOWNLOAD_ERROR;
                SDL_strlcpy(entry->errorMessage, "Failed to promote temp file to destination", sizeof(entry->errorMessage));
                SDL_UnlockMutex(g_ModelManager.lock);
            }
        }
    } else {
        if (cancelled) {
            SDL_RemovePath(partPath);
            entry->state = MODEL_STATE_NOT_DOWNLOADED;
        } else {
            entry->state = MODEL_STATE_DOWNLOAD_ERROR;
            snprintf(entry->errorMessage, sizeof(entry->errorMessage), "Curl failed: %s", curl_easy_strerror(res));
        }
        SDL_UnlockMutex(g_ModelManager.lock);
    }
    
    SDL_SetAtomicInt(&g_DownloadFinished, 1);
    return 0;
}

void modelManagerPoll(void) {
    if (g_ModelManager.fetchInProgress && SDL_GetAtomicInt(&g_FetchFinished)) {
        int status;
        SDL_WaitThread(g_FetchThread, &status);
        g_FetchThread = NULL;
        
        g_ModelManager.fetchInProgress = false;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Model catalog fetch thread joined (status: %d)", status);
    }
    
    if (g_ActiveDownloadIndex != -1 && SDL_GetAtomicInt(&g_DownloadFinished)) {
        int status;
        SDL_WaitThread(g_DownloadThread, &status);
        g_DownloadThread = NULL;
        
        g_ActiveDownloadIndex = -1;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Model download thread joined (status: %d)", status);
    }
}

bool modelManagerStartDownload(int index) {
    if (g_ActiveDownloadIndex != -1) {
        return false; // Already downloading
    }
    
    g_ActiveDownloadIndex = index;
    SDL_SetAtomicInt(&g_DownloadCancelFlag, 0);
    SDL_SetAtomicInt(&g_DownloadFinished, 0);
    
    SDL_LockMutex(g_ModelManager.lock);
    ModelEntry* entry = &g_ModelManager.models[index];
    entry->state = MODEL_STATE_DOWNLOADING;
    SDL_SetAtomicInt(&entry->progressPercent, 0);
    entry->errorMessage[0] = '\0';
    SDL_UnlockMutex(g_ModelManager.lock);
    
    g_DownloadThread = SDL_CreateThread(downloadThreadFunc, "ModelDownload", (void*)(uintptr_t)index);
    if (!g_DownloadThread) {
        g_ActiveDownloadIndex = -1;
        SDL_LockMutex(g_ModelManager.lock);
        entry->state = MODEL_STATE_DOWNLOAD_ERROR;
        SDL_strlcpy(entry->errorMessage, "Failed to spawn download thread", sizeof(entry->errorMessage));
        SDL_UnlockMutex(g_ModelManager.lock);
        return false;
    }
    
    return true;
}

void modelManagerCancelDownload(void) {
    SDL_SetAtomicInt(&g_DownloadCancelFlag, 1);
}

bool modelManagerDeleteModel(int index, const char* activeModelFilename) {
    SDL_LockMutex(g_ModelManager.lock);
    ModelEntry* entry = &g_ModelManager.models[index];
    
    if (entry->state != MODEL_STATE_DOWNLOADED) {
        SDL_UnlockMutex(g_ModelManager.lock);
        return false;
    }
    
    if (activeModelFilename && strcmp(entry->filename, activeModelFilename) == 0) {
        SDL_UnlockMutex(g_ModelManager.lock);
        return false; // Active model protection
    }
    
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "%smodels/%s", SDL_GetBasePath(), entry->filename);
    SDL_RemovePath(fullPath);
    
    entry->state = MODEL_STATE_NOT_DOWNLOADED;
    SDL_SetAtomicInt(&entry->progressPercent, 0);
    SDL_UnlockMutex(g_ModelManager.lock);
    return true;
}

bool modelManagerIsDownloading(void) {
    return g_ActiveDownloadIndex != -1;
}

void modelManagerRescanLocal(void) {
    SDL_LockMutex(g_ModelManager.lock);
    
    // Only rebuild/clear the array if there is NO active download running
    if (g_ActiveDownloadIndex == -1) {
        g_ModelManager.count = 0;
        g_ModelManager.catalogFetched = false;
        memset(g_ModelManager.models, 0, sizeof(g_ModelManager.models));
    } else {
        // If a download is active, just reset downloaded states in-place to protect indices
        for (int i = 0; i < g_ModelManager.count; ++i) {
            if (i != g_ActiveDownloadIndex && g_ModelManager.models[i].state == MODEL_STATE_DOWNLOADED) {
                g_ModelManager.models[i].state = MODEL_STATE_NOT_DOWNLOADED;
            }
        }
    }
    
    SDL_UnlockMutex(g_ModelManager.lock);
    
    scanLocalModels();
}
