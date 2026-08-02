#include "controlPanel_internal.h"

static void formatEtaString(int etaSeconds, char *dest, size_t destSize) {
    if (etaSeconds < 0) {
        SDL_strlcpy(dest, "?", destSize);
    } else if (etaSeconds < 3600) {
        (void)SDL_snprintf(dest, destSize, "%dm %02ds", etaSeconds / 60, etaSeconds % 60);
    } else {
        (void)SDL_snprintf(dest, destSize, "%dh %02dm %02ds", etaSeconds / 3600, (etaSeconds % 3600) / 60, etaSeconds % 60);
    }
}

static int compareLangIds(const void *a, const void *b) {
    int idA = *(const int *)a;
    int idB = *(const int *)b;
    const char *nameA = whisper_lang_str_full(idA);
    const char *nameB = whisper_lang_str_full(idB);
    if (!nameA)
        return -1;
    if (!nameB)
        return 1;
    return strcasecmp(nameA, nameB);
}

void renderTranscriptionPage(const char *activeModelFilename, bool *triggerDeletePopup) {
    // Model Selection
    ModelManager *mm = getModelManager();
    SDL_LockMutex(mm->lock);

    // Check for download errors to show automatic popups
    for (int i = 0; i < mm->count; i++) {
        if (mm->models[i].state == MODEL_STATE_DOWNLOAD_ERROR) {
            triggerGlobalError("Download failed for %s:\n%s", mm->models[i].name, mm->models[i].errorMessage);
            mm->models[i].state = MODEL_STATE_NOT_DOWNLOADED;
            mm->models[i].errorMessage[0] = '\0';
        }
    }

    // Check for catalog fetch errors to show automatic popups
    if (mm->catalogErrorMessage[0] != '\0') {
        const char *tip = "";

        // Detect common offline/network errors to append inline tips
        if (SDL_strstr(mm->catalogErrorMessage, "resolve") != NULL || SDL_strstr(mm->catalogErrorMessage, "connect") != NULL ||
            SDL_strstr(mm->catalogErrorMessage, "timeout") != NULL) {
            tip = "\n\nTip: Please check your Wi-Fi or internet connection.";
        } else if (SDL_strstr(mm->catalogErrorMessage, "parse") != NULL) {
            tip = "\n\nTip: This can happen if your network requires a login portal (e.g. public Wi-Fi). Please check your browser.";
        }

        triggerGlobalError("Failed to fetch model catalog:\n%s%s", mm->catalogErrorMessage, tip);
        mm->catalogErrorMessage[0] = '\0';
    }

    const char *modelDisplayName = getFilenameFromPath(uiConfig.modelPath);
    char comboLabel[256];
    SDL_strlcpy(comboLabel, modelDisplayName, sizeof(comboLabel));
    ModelEntry *selectedEntry = NULL;

    for (int i = 0; i < mm->count; i++) {
        if (strcmp(mm->models[i].filename, modelDisplayName) == 0) {
            selectedEntry = &mm->models[i];
            (void)snprintf(comboLabel, sizeof(comboLabel), "%s (%.1f MB)", mm->models[i].name, (double)mm->models[i].remoteSize / (1024.0 * 1024.0));
            break;
        }
    }

    igAlignTextToFramePadding();
    igText("Model");
    igSameLine(180.0f, 0.0f);

    igSetNextItemWidth(-34.0f);
    bool comboOpen = igBeginCombo("##Model", comboLabel, 0);
    if (igIsItemHovered(0) && selectedEntry && selectedEntry->state == MODEL_STATE_DOWNLOADING) {
        int pct = SDL_GetAtomicInt(&selectedEntry->progressPercent);
        int etaSec = SDL_GetAtomicInt(&selectedEntry->etaSeconds);
        char etaBuf[64];
        formatEtaString(etaSec, etaBuf, sizeof(etaBuf));
        igSetTooltip("Downloading (%02d%%)\nETA: %s", pct, etaBuf);
    }
    if (comboOpen) {
        if (mm->count == 0) {
            if (mm->fetchInProgress) {
                igSelectable_Bool("Loading catalog...##empty", false, ImGuiSelectableFlags_Disabled, (ImVec2_c){0, 0});
            } else {
                igSelectable_Bool("Catalog empty / Offline##empty", false, ImGuiSelectableFlags_Disabled, (ImVec2_c){0, 0});
            }
        } else {
            bool isNonEnglish = (strcmp(uiConfig.language, "auto") != 0 && strcmp(uiConfig.language, "en") != 0);
            for (int i = 0; i < mm->count; i++) {
                ModelEntry *entry = &mm->models[i];
                if (isNonEnglish && strstr(entry->filename, ".en") != NULL) {
                    continue;
                }
                bool isSelected = (strcmp(modelDisplayName, entry->filename) == 0);
                bool isActive = (strcmp(activeModelFilename, entry->filename) == 0);

                char itemDisplay[256];
                (void)snprintf(itemDisplay, sizeof(itemDisplay), "%s (%.1f MB)", entry->name, (double)entry->remoteSize / (1024.0 * 1024.0));

                igPushID_Int(i);

                bool rowClicked = igSelectable_Bool(itemDisplay, isSelected, ImGuiSelectableFlags_NoAutoClosePopups, (ImVec2_c){0.0f, 24.0f});
                if (igIsItemHovered(0) && entry->state == MODEL_STATE_DOWNLOADING) {
                    int pct = SDL_GetAtomicInt(&entry->progressPercent);
                    int etaSec = SDL_GetAtomicInt(&entry->etaSeconds);
                    char etaBuf[64];
                    formatEtaString(etaSec, etaBuf, sizeof(etaBuf));
                    igSetTooltip("Downloading (%2d%%)\nETA: %s", pct, etaBuf);
                }

                ImVec2_c minVal = igGetItemRectMin();
                ImVec2_c maxVal = igGetItemRectMax();
                ImDrawList *drawList = igGetWindowDrawList();

                // Draw border around the entire row for all on-disk models
                if (entry->state == MODEL_STATE_DOWNLOADED) {
                    ImU32 borderCol = igGetColorU32_Col(ImGuiCol_Border, 1.0f);
                    ImDrawList_AddRect(drawList, minVal, maxVal, borderCol, 0.0f, 1.0f, 0);
                }

                // Progress bar background for active download/verify state
                if (entry->state == MODEL_STATE_DOWNLOADING || entry->state == MODEL_STATE_VERIFYING) {
                    float pct = (entry->state == MODEL_STATE_DOWNLOADING) ? (float)SDL_GetAtomicInt(&entry->progressPercent) / 100.0f : 1.0f;
                    float rowWidth = maxVal.x - minVal.x;
                    ImVec2_c progressMax = {minVal.x + rowWidth * pct, maxVal.y};
                    ImU32 barCol = igGetColorU32_Col(ImGuiCol_Header, 0.4f);
                    ImDrawList_AddRectFilled(drawList, minVal, progressMax, barCol, 0.0f, 0);
                }

                // Align action text icon on the far right of the selectable row container
                const char *iconStr = "";
                ImU32 iconCol = 0xFFFFFFFF;

                if (entry->state == MODEL_STATE_DOWNLOADED) {
                    if (isActive) {
                        iconCol = igGetColorU32_Vec4((ImVec4_c){0.3f, 1.0f, 0.3f, 1.0f});
                        iconStr = "[A]";
                    } else {
                        iconCol = igGetColorU32_Vec4((ImVec4_c){1.0f, 0.3f, 0.3f, 1.0f});
                        iconStr = "[D]";
                    }
                } else if (entry->state == MODEL_STATE_DOWNLOADING) {
                    iconCol = igGetColorU32_Vec4((ImVec4_c){1.0f, 1.00f, 1.00f, 1.00f});
                    iconStr = "[X]";
                } else if (entry->state == MODEL_STATE_NOT_DOWNLOADED) {
                    iconCol = igGetColorU32_Vec4((ImVec4_c){0.6f, 0.6f, 0.6f, 1.00f});
                    iconStr = "[+]";
                }

                if (iconStr[0] != '\0') {
                    float iconWidth = igCalcTextSize(iconStr, NULL, false, -1.0f).x;
                    float iconX = maxVal.x - iconWidth - 8.0f;
                    ImVec2_c iconPos = {iconX, minVal.y + 7.0f};
                    ImDrawList_AddText_Vec2(drawList, iconPos, iconCol, iconStr, NULL);
                }

                if (rowClicked) {
                    ImVec2_c mousePos = igGetMousePos();
                    bool clickedIcon = (mousePos.x >= maxVal.x - 36.0f);
                    if (clickedIcon) {
                        // Action Triggered
                        if (entry->state == MODEL_STATE_DOWNLOADED) {
                            if (!isActive) {
                                g_DeleteTargetIndex = i;
                                *triggerDeletePopup = true;
                            }
                        } else if (entry->state == MODEL_STATE_DOWNLOADING) {
                            modelManagerCancelDownload();
                        } else if (entry->state == MODEL_STATE_NOT_DOWNLOADED) {
                            modelManagerStartDownload(i);
                        }
                    } else {
                        // Selection Triggered - ONLY if already downloaded
                        if (entry->state == MODEL_STATE_DOWNLOADED) {
                            (void)snprintf(uiConfig.modelPath, sizeof(uiConfig.modelPath), "models/%s", entry->filename);
                            igCloseCurrentPopup();
                        }
                    }
                }

                if (isSelected) {
                    igSetItemDefaultFocus();
                }

                igPopID();
            }
        }
        igEndCombo();
    }
    igSameLine(0.0f, 4.0f);
    bool isBusy = mm->fetchInProgress || modelManagerIsDownloading();
    igBeginDisabled(isBusy);
    if (igButton("R", (ImVec2_c){30.0f, 0.0f})) {
        modelManagerRescanLocal();
        modelManagerStartFetchCatalog();
    }
    igEndDisabled();
    if (igIsItemHovered(0)) {
        igSetTooltip("Reload model list");
    }
    if (*triggerDeletePopup) {
        igOpenPopup_Str("Confirm Deletion##Modal", 0);
    }

    SDL_UnlockMutex(mm->lock);

    // GPU Toggle & CPU Thread Count
    bool hasGpu = whisperHasGpu();
    if (!hasGpu) {
        uiConfig.use_gpu = false;
    }

    igBeginDisabled(!hasGpu);
    igAlignTextToFramePadding();
    igText("Use GPU (Vulkan)");
    igSameLine(180.0f, 0.0f);
    igCheckbox("##use_gpu", &uiConfig.use_gpu);
    igEndDisabled();
    if (!hasGpu && igIsItemHovered(0)) {
        igSetTooltip("No compatible Vulkan GPU found on this system");
    }

    // CPU Thread Count (faded out when running on GPU)
    int maxThreads = SDL_GetNumLogicalCPUCores();
    if (maxThreads < 1) {
        maxThreads = 1;
    }
    int tempThreads = uiConfig.cpu_threads;

    igSameLine(230.0f, 0.0f);
    igBeginDisabled(uiConfig.use_gpu);
    igAlignTextToFramePadding();
    igText("CPU Threads");
    igSameLine(340.0f, 0.0f);
    igSetNextItemWidth(-1.0f);
    igPushStyleColor_Vec4(ImGuiCol_SliderGrab, (ImVec4_c){0.85f, 0.15f, 0.15f, 1.00f});
    igPushStyleColor_Vec4(ImGuiCol_SliderGrabActive, (ImVec4_c){1.00f, 0.25f, 0.25f, 1.00f});
    igPushStyleColor_Vec4(ImGuiCol_FrameBgActive, (ImVec4_c){0.30f, 0.05f, 0.05f, 1.00f});
    if (igSliderInt("##cpu_threads", &tempThreads, 1, maxThreads, "%d", 0)) {
        uiConfig.cpu_threads = tempThreads;
    }
    igPopStyleColor(3);
    igEndDisabled();

    // Spoken Language Selection (sorted alphabetically)
    igAlignTextToFramePadding();
    igText("Spoken Language");
    igSameLine(180.0f, 0.0f);

    int maxLangId = whisper_lang_max_id();
    int langIds[128] = {0};
    int langCount = 0;

    for (int i = 0; i < maxLangId && langCount < 128; i++) {
        if (whisper_lang_str(i) && whisper_lang_str_full(i)) {
            langIds[langCount++] = i;
        }
    }

    qsort(langIds, (size_t)langCount, sizeof(int), compareLangIds);

    char currentLangCap[64] = "Auto Detect";
    if (uiConfig.language[0] != '\0' && strcmp(uiConfig.language, "auto") != 0) {
        for (int k = 0; k < langCount; k++) {
            const char *code = whisper_lang_str(langIds[k]);
            if (code && strcmp(uiConfig.language, code) == 0) {
                const char *full = whisper_lang_str_full(langIds[k]);
                if (full) {
                    SDL_strlcpy(currentLangCap, full, sizeof(currentLangCap));
                    if (currentLangCap[0] >= 'a' && currentLangCap[0] <= 'z') {
                        currentLangCap[0] -= 32;
                    }
                }
                break;
            }
        }
    }

    igSetNextItemWidth(-1.0f);
    if (igBeginCombo("##SpokenLanguage", currentLangCap, 0)) {
        if (igSelectable_Bool("Auto Detect", strcmp(uiConfig.language, "auto") == 0, 0, (ImVec2_c){0, 0})) {
            SDL_strlcpy(uiConfig.language, "auto", sizeof(uiConfig.language));
        }

        for (int k = 0; k < langCount; k++) {
            const char *code = whisper_lang_str(langIds[k]);
            const char *full = whisper_lang_str_full(langIds[k]);
            if (!code || !full)
                continue;

            char capFull[64];
            SDL_strlcpy(capFull, full, sizeof(capFull));
            if (capFull[0] >= 'a' && capFull[0] <= 'z') {
                capFull[0] -= 32;
            }
            char displayLabel[128];
            (void)snprintf(displayLabel, sizeof(displayLabel), "%s (%s)", capFull, code);

            bool isSaved = (strcmp(uiConfig.language, code) == 0);
            if (igSelectable_Bool(displayLabel, isSaved, 0, (ImVec2_c){0, 0})) {
                SDL_strlcpy(uiConfig.language, code, sizeof(uiConfig.language));
            }
            if (isSaved) {
                igSetItemDefaultFocus();
            }
        }
        igEndCombo();
    }
}
