#include "configManager.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char firstModel[256];
} ScanFirstModelData;

static SDL_EnumerationResult SDLCALL scanFirstModelCallback(void *userdata, const char *dirname, const char *fname) {
    (void)dirname;
    ScanFirstModelData *data = (ScanFirstModelData *)userdata;

    size_t len = strlen(fname);
    if (len > 4 && strcmp(fname + len - 4, ".bin") == 0) {
        snprintf(data->firstModel, sizeof(data->firstModel), "models/%s", fname);
        return SDL_ENUM_FAILURE;
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

static char *readFileContents(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file)
        return NULL;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    if (length <= 0) {
        fclose(file);
        return NULL;
    }
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *)malloc((size_t)length + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)length, file);
    buffer[read] = '\0';
    fclose(file);
    return buffer;
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

    char *contents = readFileContents(configPath);
    if (!contents)
        return CONFIG_LOAD_FILE_NOT_FOUND;

    cJSON *root = cJSON_Parse(contents);
    free(contents);
    if (!root) {
        char backupPath[520];
        snprintf(backupPath, sizeof(backupPath), "%s.bak", configPath);
        rename(configPath, backupPath);
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
    cJSON_AddItemToObject(root, "text_color", colorToJson(conf->text_color));
    cJSON_AddItemToObject(root, "text_outline_color", colorToJson(conf->text_outline_color));
    cJSON_AddStringToObject(root, "modelPath", conf->modelPath);
    cJSON_AddBoolToObject(root, "use_gpu", conf->use_gpu);

    char *jsonStr = cJSON_Print(root);
    cJSON_Delete(root);
    if (!jsonStr)
        return false;

    FILE *file = fopen(configPath, "w");
    if (!file) {
        free(jsonStr);
        return false;
    }

    fputs(jsonStr, file);
    fclose(file);
    free(jsonStr);
    return true;
}

AppConfig loadDefaultConfig(void) {
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