#include "configManager.h"
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

static SDL_Color parseColorObject(const cJSON *obj, SDL_Color fallback) {
    if (!cJSON_IsObject(obj))
        return fallback;

    SDL_Color c = fallback;
    const cJSON *r = cJSON_GetObjectItemCaseSensitive(obj, "r");
    const cJSON *g = cJSON_GetObjectItemCaseSensitive(obj, "g");
    const cJSON *b = cJSON_GetObjectItemCaseSensitive(obj, "b");

    if (cJSON_IsNumber(r))
        c.r = (Uint8)r->valueint;
    if (cJSON_IsNumber(g))
        c.g = (Uint8)g->valueint;
    if (cJSON_IsNumber(b))
        c.b = (Uint8)b->valueint;
    return c;
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
        char backupPath[sizeof(configPath) + 4];
        int res = snprintf(backupPath, sizeof(backupPath), "%s.bak", configPath);
        if (res >= 0) {
            SDL_RenamePath(configPath, backupPath);
        }
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to parse config JSON, backed up broken file to %s", backupPath);
        return CONFIG_LOAD_PARSE_ERROR;
    }

    const cJSON *item;

    item = cJSON_GetObjectItemCaseSensitive(root, "font");
    if (cJSON_IsString(item) && item->valuestring) {
        SDL_strlcpy(conf->font, item->valuestring, sizeof(conf->font));
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "font_size");
    if (cJSON_IsNumber(item)) {
        conf->font_size = item->valueint;
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "outline_thickness");
    if (cJSON_IsNumber(item)) {
        conf->outline_thickness = item->valueint;
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "display_mode");
    if (cJSON_IsNumber(item)) {
        conf->display_mode = item->valueint;
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "text_color");
    if (cJSON_IsObject(item)) {
        conf->text_color = parseColorObject(item, conf->text_color);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "text_outline_color");
    if (cJSON_IsObject(item)) {
        conf->text_outline_color = parseColorObject(item, conf->text_outline_color);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "modelPath");
    if (cJSON_IsString(item) && item->valuestring) {
        SDL_strlcpy(conf->modelPath, item->valuestring, sizeof(conf->modelPath));
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "use_gpu");
    if (cJSON_IsBool(item)) {
        conf->use_gpu = cJSON_IsTrue(item);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "cpu_threads");
    if (cJSON_IsNumber(item)) {
        conf->cpu_threads = item->valueint;
    }

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

    char *jsonStr = cJSON_Print(root);
    cJSON_Delete(root);
    if (!jsonStr)
        return false;

    FILE *file = fopen(configPath, "w");
    if (!file) {
        cJSON_free(jsonStr);
        return false;
    }

    bool writeOk = (fputs(jsonStr, file) != EOF);
    bool closeOk = (fclose(file) == 0);
    cJSON_free(jsonStr);

    return writeOk && closeOk;
}

AppConfig loadDefaultConfig(void) {
    AppConfig conf = {
        .font = "fonts/cascadia.mono.ttf",
        .font_size = 24,
        .outline_thickness = 4,
        .display_mode = 0,
        .text_color = {255, 255, 255, 255},
        .text_outline_color = {0, 0, 0, 255},
        .use_gpu = true,
        .cpu_threads = SDL_GetNumLogicalCPUCores() / 2,
    };

    if (conf.cpu_threads < 1) {
        conf.cpu_threads = 1;
    }

    getFirstLocalModelPath(conf.modelPath, sizeof(conf.modelPath));

    return conf;
}
