#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#define MODEL_NAME_MAX 128
#define MODEL_MAX_COUNT 64

typedef enum {
    MODEL_STATE_NOT_DOWNLOADED,   // Local file does not exist
    MODEL_STATE_DOWNLOADING,      // Download in progress
    MODEL_STATE_VERIFYING,        // Running SHA256 integrity check
    MODEL_STATE_DOWNLOADED,       // Local file exists and is valid
    MODEL_STATE_DOWNLOAD_ERROR    // Last download attempt failed
} ModelState;

typedef struct {
    char name[MODEL_NAME_MAX];        // e.g. "base.en-q5_1"
    char filename[MODEL_NAME_MAX];    // e.g. "ggml-base.en-q5_1.bin"
    int64_t remoteSize;               // Size in bytes from HF API
    char oid[65];                     // SHA256 LFS OID from HF API
    ModelState state;
    SDL_AtomicInt progressPercent;    // Progress percentage tracker (0 to 100, atomic)
    char errorMessage[128];           // Populated on MODEL_STATE_DOWNLOAD_ERROR
} ModelEntry;

typedef struct {
    ModelEntry models[MODEL_MAX_COUNT];
    int count;
    bool catalogFetched;             // True after a successful API fetch
    bool fetchInProgress;            // True while the catalog fetch thread is running
    char catalogErrorMessage[128];   // Error message populated on catalog fetch failure
    SDL_Mutex* lock;                 // Synchronize access to models catalog
} ModelManager;

void modelManagerInit(void);
void modelManagerShutdown(void);

void modelManagerStartFetchCatalog(void);
void modelManagerPoll(void);
void modelManagerRescanLocal(void);

ModelManager* getModelManager(void);

// Download and delete operations
bool modelManagerStartDownload(int index);
void modelManagerCancelDownload(void);
bool modelManagerDeleteModel(int index, const char* activeModelFilename);
bool modelManagerIsDownloading(void);

#endif // MODEL_MANAGER_H
