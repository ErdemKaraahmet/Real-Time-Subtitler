#include "loadConfig.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char firstModel[256];
} ScanFirstModelData;

static SDL_EnumerationResult SDLCALL scanFirstModelCallback(void* userdata, const char* dirname, const char* fname) {
    (void)dirname;
    ScanFirstModelData* data = (ScanFirstModelData*)userdata;
    
    size_t len = strlen(fname);
    if (len > 4 && strcmp(fname + len - 4, ".bin") == 0) {
        snprintf(data->firstModel, sizeof(data->firstModel), "models/%s", fname);
        return SDL_ENUM_FAILURE; // Stop scanning on first match
    }
    return SDL_ENUM_CONTINUE;
}

static void getFirstLocalModelPath(char* dest, size_t destSize) {
    char modelsPath[512];
    const char* basePath = SDL_GetBasePath();
    snprintf(modelsPath, sizeof(modelsPath), "%smodels", basePath);
    
    ScanFirstModelData data = {0};
    SDL_EnumerateDirectory(modelsPath, scanFirstModelCallback, &data);
    
    if (data.firstModel[0] != '\0') {
        SDL_strlcpy(dest, data.firstModel, destSize);
    } else {
        dest[0] = '\0'; // Return empty string if no model is found
    }
}

// returns CONFIG_LOAD_FILE_NOT_FOUND if file can't be opened,
// CONFIG_LOAD_NONE if file exists but nothing loaded,
// CONFIG_LOAD_PARTIAL if some fields loaded,
// CONFIG_LOAD_FULL if all fields loaded
static void resolveConfigPath(char* dest, size_t destSize) {
    const char* basePath = SDL_GetBasePath();
    
    // Check if the config exists in the parent dir first (dev mode)
    snprintf(dest, destSize, "%s../config.ini", basePath);
    FILE* file = fopen(dest, "r");
    if (file) {
        fclose(file);
        return;
    }
    
    // Otherwise, default to writing in the base path next to the binary (release mode)
    snprintf(dest, destSize, "%sconfig.ini", basePath);
}

ConfigLoadStatus loadConfig(AppConfig* conf) {
    char configPath[512];
    resolveConfigPath(configPath, sizeof(configPath));

    FILE *file = fopen(configPath, "r");
    if (!file) {
        return CONFIG_LOAD_FILE_NOT_FOUND;
    }

    char line[100];
    char key[50];
    char val[50];

    int total = 0;
    int loadedCount = 0;

    // Read line by line: "key=value"
    while (fgets(line, sizeof(line), file)) {
        total++;

        if (sscanf(line, "%[^=]=%s", key, val) == 2) {
            int r, g, b;
            if (strcmp(key, "font") == 0) {
                strncpy(conf->font, val, sizeof(conf->font) - 1);
                conf->font[sizeof(conf->font) - 1] = '\0'; // Ensure null-termination
                loadedCount++;
            }
            else if (strcmp(key, "font_size") == 0) {
                conf->font_size = atoi(val);
                loadedCount++;
            }
            else if (strcmp(key, "outline_thickness") == 0) {
                conf->outline_thickness = atoi(val);
                loadedCount++;
            } 
            else if (strcmp(key, "text_color") == 0) {
                if (sscanf(val, "%d,%d,%d", &r, &g, &b) == 3) {
                    conf->text_color.r = (uint8_t)r;
                    conf->text_color.g = (uint8_t)g;
                    conf->text_color.b = (uint8_t)b;
                    loadedCount++;
                }
            } 
            else if (strcmp(key, "text_outline_color") == 0) {
                if (sscanf(val, "%d,%d,%d", &r, &g, &b) == 3) {
                    conf->text_outline_color.r = (uint8_t)r;
                    conf->text_outline_color.g = (uint8_t)g;
                    conf->text_outline_color.b = (uint8_t)b;
                    loadedCount++;
                }
            }
            else if (strcmp(key, "modelPath") == 0) {
                strncpy(conf->modelPath, val, sizeof(conf->modelPath) - 1);
                conf->modelPath[sizeof(conf->modelPath) - 1] = '\0'; // Ensure null-termination
                loadedCount++;
            }
            else if (strcmp(key, "use_gpu") == 0) {
                conf->use_gpu = (atoi(val) != 0);
                loadedCount++;
            }
        }
    }

    fclose(file);

    if (loadedCount == total && total > 0) {
        return CONFIG_LOAD_FULL;
    }
    else if (loadedCount > 0) {
        return CONFIG_LOAD_PARTIAL;
    }
    else {
        return CONFIG_LOAD_NONE;
    }
}

bool saveConfig(const AppConfig* conf) {
    char configPath[512];
    resolveConfigPath(configPath, sizeof(configPath));
    
    FILE *file = fopen(configPath, "w");
    if (!file) {
        return false; // OS denied write access
    }

    // Write formatted key=value pairs
    fprintf(file, "font=%s\n", conf->font);
    fprintf(file, "font_size=%d\n", conf->font_size);
    fprintf(file, "outline_thickness=%d\n", conf->outline_thickness);
    fprintf(file, "text_color=%d,%d,%d\n", conf->text_color.r, conf->text_color.g, conf->text_color.b);
    fprintf(file, "text_outline_color=%d,%d,%d\n", conf->text_outline_color.r, conf->text_outline_color.g, conf->text_outline_color.b);
    fprintf(file, "modelPath=%s\n", conf->modelPath);
    fprintf(file, "use_gpu=%d\n", conf->use_gpu ? 1 : 0);

    fclose(file);
    return true;
}

AppConfig loadDefaultConfig(){
    
    AppConfig conf = {
        .font = "fonts/cascadia.mono.ttf",
        .font_size = 24,
        .outline_thickness = 4,
        .text_color = {255, 255, 255, 255},
        .text_outline_color = {0, 0, 0, 255},
        .use_gpu = true,
    };

    getFirstLocalModelPath(conf.modelPath, sizeof(conf.modelPath));

    return conf;
}