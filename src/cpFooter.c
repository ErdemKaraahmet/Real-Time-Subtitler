#include "controlPanel_internal.h"

void renderFooter(ControlPanelStatus *status, bool isDirty) {
    float windowHeight = igGetWindowHeight();
    float paddingY = igGetStyle()->WindowPadding.y;
    float footerStartY = windowHeight - paddingY - 30.0f - 20.0f; // 30px button height + 20px margin

    igSetCursorPosY(footerStartY);
    igSeparator();
    igSpacing();

    // Buttons (Save & Load Defaults - right-aligned, Load Defaults on Left)
    float btnStartX = igGetWindowWidth() - (UI_BUTTON_WIDTH * 2.0f) - UI_SPACING - igGetStyle()->WindowPadding.x;
    if (btnStartX < 0.0f)
        btnStartX = 0.0f;

    igSetCursorPosX(btnStartX);

    // Load Defaults Button
    if (igButton("Load Defaults", (ImVec2_c){UI_BUTTON_WIDTH, 0.0f})) {
        uiConfig = loadDefaultConfig();
        previewNeedsUpdate = true;
    }

    igSameLine(0.0f, UI_SPACING);

    // Save Button
    if (!isDirty) {
        igBeginDisabled(true);
        igButton("Saved", (ImVec2_c){UI_BUTTON_WIDTH, 0.0f});
        igEndDisabled();
    } else {
        if (igButton("Save", (ImVec2_c){UI_BUTTON_WIDTH, 0.0f})) {
            char fontError[128] = "";
            char modelError[128] = "";
            bool fontOk = utilsIsFileReadable(uiConfig.font);
            bool modelOk = utilsIsFileReadable(uiConfig.modelPath);

            if (!fontOk) {
                SDL_strlcpy(fontError, "Font unreadable, using fallback", sizeof(fontError));
            }
            if (!modelOk) {
                SDL_strlcpy(modelError, "Model unreadable, make sure you select one", sizeof(modelError));
            }

            if (!fontOk || !modelOk) {
                if (!fontOk && !modelOk) {
                    triggerGlobalError("%s\n%s", fontError, modelError);
                } else if (!fontOk) {
                    triggerGlobalError("%s", fontError);
                } else {
                    triggerGlobalError("%s", modelError);
                }
            } else if (saveConfig(&uiConfig)) {
                if (pLiveConfig) {
                    *pLiveConfig = uiConfig;
                }
                status->configSaved = true;
                if (strcmp(uiConfig.modelPath, savedConfig.modelPath) != 0 || uiConfig.use_gpu != savedConfig.use_gpu || whisperStatusError) {
                    status->modelChanged = true;
                }
                savedConfig = uiConfig;
                SDL_strlcpy(whisperStatusMessage, "Status: Active (Config Saved)", sizeof(whisperStatusMessage));
                whisperStatusError = false;
            } else {
                triggerGlobalError("Failed to write config");
            }
        }
    }
}
