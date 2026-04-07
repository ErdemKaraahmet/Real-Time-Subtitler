#include "loadConfig.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

AppConfig loadConfig() {
      
    // Default values
    AppConfig conf = {
        .font_size = 36,
        .outline_thickness = 4,
        .text_color = {255, 255, 255, 255},
        .text_outline_color = {0, 0, 0, 255},
        .modelPath = "models/ggml-base.en.bin"  // default
    };

    // Search parent directory
    char configPath[512];
    const char* basePath = SDL_GetBasePath();
    snprintf(configPath, sizeof(configPath), "%s../config.ini", basePath);
    SDL_free((void*)basePath);

    FILE *file = fopen(configPath, "r");

    // Search basepath if its not in parent dir
    if (!file) {
        snprintf(configPath, sizeof(configPath), "%sconfig.ini", basePath);
        file = fopen(configPath, "r");
        if(!file){
            SDL_Log("Couldnt open config file");
            return conf; // return default
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
                conf.font_size = atoi(val);
            }
            else if (strcmp(key, "outline_thickness") == 0) { // Get outline thickness
                conf.outline_thickness = atoi(val);
            } 
            else if (strcmp(key, "text_color") == 0) {
                if (sscanf(val, "%d,%d,%d", &r, &g, &b) == 3) {
                    conf.text_color.r = r;
                    conf.text_color.g = g;
                    conf.text_color.b = b;
                }
            } 
            else if (strcmp(key, "text_outline_color") == 0) {
                if (sscanf(val, "%d,%d,%d", &r, &g, &b) == 3) {
                    conf.text_outline_color.r = r;
                    conf.text_outline_color.g = g;
                    conf.text_outline_color.b = b;
                }
            }
            else if (strcmp(key, "modelPath") == 0) {
                strncpy(conf.modelPath, val, sizeof(conf.modelPath) - 1);
            }
        }
    }

    fclose(file);
    return conf;
}