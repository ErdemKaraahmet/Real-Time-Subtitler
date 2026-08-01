#include "configManager.h"
#include "whisperEngine.h"
#include <SDL3/SDL.h>
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char firstModel[256];
} ScanFirstModelData;

static SDL_EnumerationResult SDLCALL scanFirstModelCallback(void *userdata, const char *dirname, const char *fname) {
    (void)dirname;

    size_t len = strlen(fname);
    if (len > 4 && strcmp(fname + len - 4, ".bin") == 0) {
        ScanFirstModelData *data = userdata;
        int res = snprintf(data->firstModel, sizeof(data->firstModel), "models/%s", fname);
        if (res >= 0 && (size_t)res < sizeof(data->firstModel)) {
            return SDL_ENUM_SUCCESS;
        }
    }
    return SDL_ENUM_CONTINUE;
}

static void getFirstLocalModelPath(char *dest, size_t destSize) {
    char modelsPath[512];
    utilsResolvePath(modelsPath, sizeof(modelsPath), "models");

    ScanFirstModelData data = {0};
    SDL_EnumerateDirectory(modelsPath, scanFirstModelCallback, &data);

    if (data.firstModel[0] != '\0') {
        SDL_strlcpy(dest, data.firstModel, destSize);
    } else {
        dest[0] = '\0';
    }
}

static void resolveConfigPath(char *dest, size_t destSize) {
    utilsResolvePath(dest, destSize, "config.json");
}

static void getJsonString(const cJSON *root, const char *key, char *dest, size_t destSize) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsString(item) && item->valuestring && item->valuestring[0] != '\0') {
        size_t len = strlen(item->valuestring);
        if (len >= destSize) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Config key '%s' string length exceeded limit (%zu >= %zu bytes); keeping default value", key,
                        len, destSize);
        } else {
            SDL_strlcpy(dest, item->valuestring, destSize);
        }
    }
}

static void getJsonIntClamped(const cJSON *root, const char *key, int *val, int minVal, int maxVal) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsNumber(item)) {
        int v = item->valueint;
        if (v < minVal)
            v = minVal;
        if (v > maxVal)
            v = maxVal;
        *val = v;
    }
}

static void getJsonBool(const cJSON *root, const char *key, bool *val) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsBool(item)) {
        *val = cJSON_IsTrue(item);
    }
}

static Uint8 clampUint8(int val) {
    if (val < 0)
        return 0;
    if (val > 255)
        return 255;
    return (Uint8)val;
}

static SDL_Color parseColorObject(const cJSON *obj, SDL_Color fallback) {
    if (!cJSON_IsObject(obj))
        return fallback;

    SDL_Color c = fallback;
    const cJSON *r = cJSON_GetObjectItemCaseSensitive(obj, "r");
    const cJSON *g = cJSON_GetObjectItemCaseSensitive(obj, "g");
    const cJSON *b = cJSON_GetObjectItemCaseSensitive(obj, "b");

    if (cJSON_IsNumber(r))
        c.r = clampUint8(r->valueint);
    if (cJSON_IsNumber(g))
        c.g = clampUint8(g->valueint);
    if (cJSON_IsNumber(b))
        c.b = clampUint8(b->valueint);
    return c;
}

static void getJsonColor(const cJSON *root, const char *key, SDL_Color *color) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (item) {
        *color = parseColorObject(item, *color);
    }
}

static cJSON *colorToJson(SDL_Color c) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj)
        return NULL;
    cJSON_AddNumberToObject(obj, "r", c.r);
    cJSON_AddNumberToObject(obj, "g", c.g);
    cJSON_AddNumberToObject(obj, "b", c.b);
    return obj;
}

ConfigLoadStatus loadConfig(AppConfig *conf) {
    char configPath[512];
    resolveConfigPath(configPath, sizeof(configPath));

    char *contents = (char *)SDL_LoadFile(configPath, NULL);
    if (!contents)
        return CONFIG_LOAD_FILE_NOT_FOUND;

    cJSON *root = cJSON_Parse(contents);
    SDL_free(contents);
    if (!root) {
        char backupPath[sizeof(configPath) + 5];
        int res = snprintf(backupPath, sizeof(backupPath), "%s.bak", configPath);
        if (res >= 0) {
            SDL_RenamePath(configPath, backupPath);
        }
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to parse config JSON, backed up broken file to %s", backupPath);
        return CONFIG_LOAD_PARSE_ERROR;
    }

    int maxThreads = SDL_GetNumLogicalCPUCores();
    if (maxThreads < 1) {
        maxThreads = 1;
    }

    getJsonString(root, "font", conf->font, sizeof(conf->font));
    getJsonIntClamped(root, "font_size", &conf->font_size, 8, 72);
    getJsonIntClamped(root, "outline_thickness", &conf->outline_thickness, 0, 20);
    getJsonIntClamped(root, "display_mode", &conf->display_mode, 0, 1);
    getJsonColor(root, "text_color", &conf->text_color);
    getJsonColor(root, "text_outline_color", &conf->text_outline_color);
    getJsonString(root, "modelPath", conf->modelPath, sizeof(conf->modelPath));
    getJsonBool(root, "use_gpu", &conf->use_gpu);
    getJsonIntClamped(root, "cpu_threads", &conf->cpu_threads, 1, maxThreads);
    getJsonString(root, "language", conf->language, sizeof(conf->language));
    getJsonIntClamped(root, "window_x", &conf->window_x, -32768, 32767);
    getJsonIntClamped(root, "window_y", &conf->window_y, -32768, 32767);

    cJSON_Delete(root);
    return CONFIG_LOAD_OK;
}

bool saveConfig(const AppConfig *conf) {
    char configPath[512];
    resolveConfigPath(configPath, sizeof(configPath));

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return false;

    cJSON_AddStringToObject(root, "font", conf->font);
    cJSON_AddNumberToObject(root, "font_size", conf->font_size);
    cJSON_AddNumberToObject(root, "outline_thickness", conf->outline_thickness);
    cJSON_AddNumberToObject(root, "display_mode", conf->display_mode);
    cJSON_AddItemToObject(root, "text_color", colorToJson(conf->text_color));
    cJSON_AddItemToObject(root, "text_outline_color", colorToJson(conf->text_outline_color));
    cJSON_AddStringToObject(root, "modelPath", conf->modelPath);
    cJSON_AddBoolToObject(root, "use_gpu", conf->use_gpu);
    cJSON_AddNumberToObject(root, "cpu_threads", conf->cpu_threads);
    cJSON_AddStringToObject(root, "language", conf->language);
    cJSON_AddNumberToObject(root, "window_x", conf->window_x);
    cJSON_AddNumberToObject(root, "window_y", conf->window_y);

    char *jsonStr = cJSON_Print(root);
    cJSON_Delete(root);
    if (!jsonStr)
        return false;

    char tmpPath[sizeof(configPath) + 5];
    if (snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", configPath) < 0) {
        cJSON_free(jsonStr);
        return false;
    }

    size_t len = strlen(jsonStr);
    bool saved = SDL_SaveFile(tmpPath, jsonStr, len);
    cJSON_free(jsonStr);

    if (!saved)
        return false;

    return SDL_RenamePath(tmpPath, configPath);
}

AppConfig loadDefaultConfig(void) {
    AppConfig conf = {
        .font = "fonts/cascadia.mono.ttf",
        .font_size = 24,
        .outline_thickness = 4,
        .display_mode = 0,
        .text_color = {255, 255, 255, 255},
        .text_outline_color = {0, 0, 0, 255},
        .use_gpu = whisperHasGpu(),
        .cpu_threads = SDL_GetNumLogicalCPUCores() / 2,
        .language = "auto",
        .window_x = -1,
        .window_y = -1,
    };

    if (conf.cpu_threads < 1) {
        conf.cpu_threads = 1;
    }

    getFirstLocalModelPath(conf.modelPath, sizeof(conf.modelPath));

    return conf;
}
