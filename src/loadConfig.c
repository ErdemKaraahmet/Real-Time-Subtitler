#include "loadConfig.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// returns false on default values, true on successful load
bool loadConfig(AppConfig* conf) {

    // Search parent directory
    char configPath[512];
    const char* basePath = SDL_GetBasePath();
    snprintf(configPath, sizeof(configPath), "%s../config.ini", basePath);

    FILE *file = fopen(configPath, "r");

    // Search basepath if its not in parent dir
    if (!file) {
        snprintf(configPath, sizeof(configPath), "%sconfig.ini", basePath);
        file = fopen(configPath, "r");
        if(!file){
            return false; // return default
        }
    }

    char line[100];
    char key[50];
    char val[50];

    // Read line by line: "key=value"
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%[^=]=%s", key, val) == 2) {
            int r, g, b;
            if (strcmp(key, "font_size") == 0) { // Get font size
                conf->font_size = atoi(val);
            }
            else if (strcmp(key, "outline_thickness") == 0) { // Get outline thickness
                conf->outline_thickness = atoi(val);
            } 
            else if (strcmp(key, "text_color") == 0) {
                if (sscanf(val, "%d,%d,%d", &r, &g, &b) == 3) {
                    conf->text_color.r = (uint8_t)r;
                    conf->text_color.g = (uint8_t)g;
                    conf->text_color.b = (uint8_t)b;
                }
            } 
            else if (strcmp(key, "text_outline_color") == 0) {
                if (sscanf(val, "%d,%d,%d", &r, &g, &b) == 3) {
                    conf->text_outline_color.r = (uint8_t)r;
                    conf->text_outline_color.g = (uint8_t)g;
                    conf->text_outline_color.b = (uint8_t)b;
                }
            }
            else if (strcmp(key, "modelPath") == 0) {
                strncpy(conf->modelPath, val, sizeof(conf->modelPath) - 1);
            }
        }
    }

    fclose(file);
    return true;
}

bool saveConfig(const AppConfig* conf) {

    char configPath[512];
    const char* basePath = SDL_GetBasePath();

    // Check if the config exists in the parent dir first, so we overwrite the correct one
    snprintf(configPath, sizeof(configPath), "%s../config.ini", basePath);
    FILE *file = fopen(configPath, "r");

    if (file) {
        fclose(file);
    } else {
        // Otherwise, default to writing in the base path
        snprintf(configPath, sizeof(configPath), "%sconfig.ini", basePath);
    }
    
    // Open the chosen path for writing
    file = fopen(configPath, "w");

    if (!file) {
        return false; // OS denied write access
    }

    // Write formatted key=value pairs
    fprintf(file, "font_size=%d\n", conf->font_size);
    fprintf(file, "outline_thickness=%d\n", conf->outline_thickness);
    fprintf(file, "text_color=%d,%d,%d\n", conf->text_color.r, conf->text_color.g, conf->text_color.b);
    fprintf(file, "text_outline_color=%d,%d,%d\n", conf->text_outline_color.r, conf->text_outline_color.g, conf->text_outline_color.b);
    fprintf(file, "modelPath=%s\n", conf->modelPath);

    fclose(file);
    return true;
}

AppConfig loadDefaultConfig(){
    
    AppConfig conf = {
        .font_size = 24,
        .outline_thickness = 4,
        .text_color = {255, 255, 255, 255},
        .text_outline_color = {0, 0, 0, 255},
        .modelPath = "models/ggml-base.en.bin",
    };

    return conf;
}