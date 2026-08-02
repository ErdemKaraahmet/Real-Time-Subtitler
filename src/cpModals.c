#include "controlPanel_internal.h"

void renderModals(const char *activeModelFilename) {
    // Render Global Error Popup Modal
    if (showGlobalUiErrorPopup) {
        ImVec2_c parentPos = igGetWindowPos();
        ImVec2_c parentSize = igGetWindowSize();
        ImVec2_c centerPos = {parentPos.x + parentSize.x * 0.5f, parentPos.y + parentSize.y * 0.5f};
        igSetNextWindowPos(centerPos, ImGuiCond_Appearing, (ImVec2_c){0.5f, 0.5f});

        igOpenPopup_Str("Error##GlobalErrorPopup", 0);
        showGlobalUiErrorPopup = false; // Reset trigger flag immediately to avoid resets
    }

    igSetNextWindowSize((ImVec2_c){360.0f, 0.0f}, ImGuiCond_Always);

    if (igBeginPopupModal("Error##GlobalErrorPopup", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        igPushTextWrapPos(igGetCursorPosX() + 328.0f); // 360px - margins
        igTextWrapped("%s", globalUiErrorMessage);
        igPopTextWrapPos();

        igSpacing();
        igSeparator();
        igSpacing();

        float okButtonPosX = igGetWindowWidth() - 120.0f - igGetStyle()->WindowPadding.x;
        if (okButtonPosX < 0.0f)
            okButtonPosX = 0.0f;
        igSetCursorPosX(okButtonPosX);
        if (igButton("OK", (ImVec2_c){120.0f, 30.0f})) {
            igCloseCurrentPopup();
        }
        igEndPopup();
    }

    // Deletion Confirmation Modal
    if (g_DeleteTargetIndex != -1) {
        ImVec2_c centerPos;
        centerPos.x = igGetIO_Nil()->DisplaySize.x * 0.5f;
        centerPos.y = igGetIO_Nil()->DisplaySize.y * 0.5f;
        igSetNextWindowPos(centerPos, ImGuiCond_Appearing, (ImVec2_c){0.5f, 0.5f});
        igSetNextWindowSize((ImVec2_c){380.0f, 0.0f}, ImGuiCond_Always);

        if (igBeginPopupModal("Confirm Deletion##Modal", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
            char modelName[256] = "";
            ModelManager *mm = getModelManager();
            SDL_LockMutex(mm->lock);
            if (g_DeleteTargetIndex < mm->count) {
                SDL_strlcpy(modelName, mm->models[g_DeleteTargetIndex].name, sizeof(modelName));
            }
            SDL_UnlockMutex(mm->lock);

            igTextWrapped("Are you sure you want to delete the model '%s'?", modelName);

            igSpacing();
            igSeparator();
            igSpacing();

            float buttonWidth = 120.0f;
            float spacing = 8.0f;
            float startX = igGetWindowWidth() - (buttonWidth * 2.0f) - spacing - igGetStyle()->WindowPadding.x;
            if (startX < 0.0f)
                startX = 0.0f;

            igSetCursorPosX(startX);
            if (igButton("Delete", (ImVec2_c){buttonWidth, 30.0f})) {
                if (g_DeleteTargetIndex != -1) {
                    modelManagerDeleteModel(g_DeleteTargetIndex, activeModelFilename);
                    g_DeleteTargetIndex = -1;
                }
                igCloseCurrentPopup();
            }
            igSameLine(0.0f, spacing);
            if (igButton("Cancel", (ImVec2_c){buttonWidth, 30.0f})) {
                g_DeleteTargetIndex = -1;
                igCloseCurrentPopup();
            }
            igEndPopup();
        }
    }

    // Font Deletion Confirmation / Protection Modal
    if (g_DeleteTargetFontFilename[0] != '\0') {
        ImVec2_c centerPos;
        centerPos.x = igGetIO_Nil()->DisplaySize.x * 0.5f;
        centerPos.y = igGetIO_Nil()->DisplaySize.y * 0.5f;
        igSetNextWindowPos(centerPos, ImGuiCond_Appearing, (ImVec2_c){0.5f, 0.5f});
        igSetNextWindowSize((ImVec2_c){400.0f, 0.0f}, ImGuiCond_Always);

        igOpenPopup_Str("Confirm Font Deletion##Modal", 0);
    }

    if (igBeginPopupModal("Confirm Font Deletion##Modal", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        bool isProtected = (SDL_strcasecmp(g_DeleteTargetFontFilename, "cascadia.mono.ttf") == 0);

        if (isProtected) {
            igPushTextWrapPos(igGetCursorPosX() + 368.0f);
            igTextWrapped("The font '%s' cannot be deleted because it is required by the Control Panel UI.", g_DeleteTargetFontFilename);
            igPopTextWrapPos();

            igSpacing();
            igSeparator();
            igSpacing();

            float buttonWidth = 120.0f;
            float startX = igGetWindowWidth() - buttonWidth - igGetStyle()->WindowPadding.x;
            if (startX < 0.0f)
                startX = 0.0f;

            igSetCursorPosX(startX);
            if (igButton("OK", (ImVec2_c){buttonWidth, 30.0f})) {
                g_DeleteTargetFontFilename[0] = '\0';
                igCloseCurrentPopup();
            }
        } else {
            igPushTextWrapPos(igGetCursorPosX() + 368.0f);
            igTextWrapped("Are you sure you want to delete the font '%s'?", g_DeleteTargetFontFilename);
            igPopTextWrapPos();

            igSpacing();
            igSeparator();
            igSpacing();

            float buttonWidth = 120.0f;
            float spacing = 8.0f;
            float startX = igGetWindowWidth() - (buttonWidth * 2.0f) - spacing - igGetStyle()->WindowPadding.x;
            if (startX < 0.0f)
                startX = 0.0f;

            igSetCursorPosX(startX);
            if (igButton("Delete", (ImVec2_c){buttonWidth, 30.0f})) {
                if (g_DeleteTargetFontFilename[0] != '\0') {
                    char fontPath[512];
                    char relPath[300];
                    (void)snprintf(relPath, sizeof(relPath), "fonts/%s", g_DeleteTargetFontFilename);
                    utilsResolvePath(fontPath, sizeof(fontPath), relPath);
                    SDL_RemovePath(fontPath);
                    rescanFonts();
                    previewNeedsUpdate = true;
                    g_DeleteTargetFontFilename[0] = '\0';
                }
                igCloseCurrentPopup();
            }
            igSameLine(0.0f, spacing);
            if (igButton("Cancel", (ImVec2_c){buttonWidth, 30.0f})) {
                g_DeleteTargetFontFilename[0] = '\0';
                igCloseCurrentPopup();
            }
        }
        igEndPopup();
    }
}
