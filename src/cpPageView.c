#include "controlPanel_internal.h"

void updatePreviewTexture(void) {
    SubtitleToken sample[3];
    strcpy(sample[0].text, "Sample");
    strcpy(sample[1].text, " Text");
    strcpy(sample[2].text, " Preview");
    sample[0].probability = 0.9f;
    sample[1].probability = 0.7f;
    sample[2].probability = 0.1f;

    if (previewTexture) {
        SDL_DestroyTexture(previewTexture);
        previewTexture = NULL;
    }

    previewFontLoadFailed = false;

    // Try to load the selected font
    TTF_Font *font = TTF_OpenFont(uiConfig.font, (float)uiConfig.font_size);
    if (!font) {
        // Fallback to default font
        font = TTF_OpenFont("fonts/cascadia.mono.ttf", (float)uiConfig.font_size);
        if (!font) {
            previewFontLoadFailed = true;
            return;
        }
    }

    // Render preview texture
    previewTexture = createTextTexture(cpRenderer, font, sample, 3, &uiConfig, &previewWidth, &previewHeight);
    TTF_CloseFont(font);
}

void renderViewPage(void) {
    // Font Selection
    const char *fontDisplayName = getFilenameFromPath(uiConfig.font);
    igAlignTextToFramePadding();
    igText("Font");
    igSameLine(180.0f, 0.0f);
    igSetNextItemWidth(-1.0f);
    if (igBeginCombo("##Font", fontDisplayName, 0)) {
        if (scannedFontCount == 0) {
            igSelectable_Bool("No item found in folder##empty_font", false, ImGuiSelectableFlags_Disabled, (ImVec2_c){0, 0});
        } else {
            for (int i = 0; i < scannedFontCount; i++) {
                bool isSelected = (strcmp(fontDisplayName, scannedFonts[i]) == 0);
                char itemDisplay[128];
                (void)snprintf(itemDisplay, sizeof(itemDisplay), "%s##font%d", scannedFonts[i], i);
                if (igSelectable_Bool(itemDisplay, isSelected, 0, (ImVec2_c){0, 0})) {
                    (void)snprintf(uiConfig.font, sizeof(uiConfig.font), "fonts/%s", scannedFonts[i]);
                    previewNeedsUpdate = true;
                }
                if (isSelected) {
                    igSetItemDefaultFocus();
                }
            }
        }
        igEndCombo();
    }

    // Font Size
    int tempFontSize = uiConfig.font_size;
    igAlignTextToFramePadding();
    igText("Font Size");
    igSameLine(180.0f, 0.0f);
    igSetNextItemWidth(-1.0f);
    if (igDragInt("##Font Size", &tempFontSize, 1.0f, 8, 72, "%d", 0)) {
        uiConfig.font_size = tempFontSize;
        previewNeedsUpdate = true;
    }

    // Outline Thickness
    int tempOutline = uiConfig.outline_thickness;
    igAlignTextToFramePadding();
    igText("Outline Thickness");
    igSameLine(180.0f, 0.0f);
    igSetNextItemWidth(-1.0f);
    if (igDragInt("##Outline Thickness", &tempOutline, 0.5f, 0, 20, "%d", 0)) {
        uiConfig.outline_thickness = tempOutline;
        previewNeedsUpdate = true;
    }

    // Color Picking
    float textColor[3] = {(float)uiConfig.text_color.r / 255.0f, (float)uiConfig.text_color.g / 255.0f, (float)uiConfig.text_color.b / 255.0f};
    igAlignTextToFramePadding();
    igText("Text Color");
    igSameLine(180.0f, 0.0f);
    igSetNextItemWidth(-1.0f);
    if (igColorEdit3("##Text Color", textColor, 0)) {
        uiConfig.text_color.r = (uint8_t)(textColor[0] * 255.0f);
        uiConfig.text_color.g = (uint8_t)(textColor[1] * 255.0f);
        uiConfig.text_color.b = (uint8_t)(textColor[2] * 255.0f);
        previewNeedsUpdate = true;
    }

    float outlineColor[3] = {(float)uiConfig.text_outline_color.r / 255.0f, (float)uiConfig.text_outline_color.g / 255.0f,
                             (float)uiConfig.text_outline_color.b / 255.0f};
    igAlignTextToFramePadding();
    igText("Outline Color");
    igSameLine(180.0f, 0.0f);
    igSetNextItemWidth(-1.0f);
    if (igColorEdit3("##Outline Color", outlineColor, 0)) {
        uiConfig.text_outline_color.r = (uint8_t)(outlineColor[0] * 255.0f);
        uiConfig.text_outline_color.g = (uint8_t)(outlineColor[1] * 255.0f);
        uiConfig.text_outline_color.b = (uint8_t)(outlineColor[2] * 255.0f);
        previewNeedsUpdate = true;
    }

    int displayModeSelection = uiConfig.display_mode;
    const char *displayModeNames[2] = {"Plain Text", "Confidence-Based Opacity"};

    igAlignTextToFramePadding();
    igText("Display Mode");
    igSameLine(180.0f, 0.0f);
    igSetNextItemWidth(-1.0f);
    if (igBeginCombo("##Display Mode", displayModeNames[displayModeSelection], 0)) {
        for (int i = 0; i < 2; ++i) {
            if (igSelectable_Bool(displayModeNames[i], (i == displayModeSelection), 0, (ImVec2_c){0, 0})) {
                displayModeSelection = i;
                uiConfig.display_mode = i;
                previewNeedsUpdate = true;
            }
        }
        igEndCombo();
    }

    igSpacing();

    // Live Preview
    igText("Live Preview:");
    ImVec2_c previewPos = igGetCursorScreenPos();

    // Draw a dark background rectangle for the preview (no rounding)
    ImDrawList *drawList = igGetWindowDrawList();
    ImDrawList_AddRectFilled(drawList, previewPos, (ImVec2_c){previewPos.x + UI_PREVIEW_BOX_WIDTH, previewPos.y + UI_PREVIEW_BOX_HEIGHT},
                             IM_COL32(15, 15, 15, 255), 0.0f, 0);

    ImDrawList_AddRect(drawList, previewPos, (ImVec2_c){previewPos.x + UI_PREVIEW_BOX_WIDTH, previewPos.y + UI_PREVIEW_BOX_HEIGHT},
                       IM_COL32(80, 80, 80, 255), 0.0f, 1.0f, 0);

    // Render the texture inside the box
    if (previewFontLoadFailed) {
        igSetCursorScreenPos((ImVec2_c){previewPos.x + 10.0f, previewPos.y + 10.0f});
        igTextColored((ImVec4_c){1.0f, 0.3f, 0.3f, 1.0f}, "Preview unavailable (No valid fonts found)");
    } else if (previewTexture) {
        // Clamp and scale preview if it exceeds bounds to prevent overflow
        float maxPreviewW = UI_PREVIEW_BOX_WIDTH - 20.0f;
        float maxPreviewH = UI_PREVIEW_BOX_HEIGHT - 20.0f;
        float displayW = previewWidth;
        float displayH = previewHeight;

        if (displayW > maxPreviewW || displayH > maxPreviewH) {
            float scaleX = maxPreviewW / displayW;
            float scaleY = maxPreviewH / displayH;
            float scale = (scaleX < scaleY) ? scaleX : scaleY;
            displayW *= scale;
            displayH *= scale;
        }

        // Center the scaled preview texture inside the box
        float startX = previewPos.x + (UI_PREVIEW_BOX_WIDTH - displayW) / 2.0f;
        float startY = previewPos.y + (UI_PREVIEW_BOX_HEIGHT - displayH) / 2.0f;

        igSetCursorScreenPos((ImVec2_c){startX, startY});
        ImTextureRef_c texRef = {NULL, (ImTextureID)(intptr_t)previewTexture};
        igImage(texRef, (ImVec2_c){displayW, displayH}, (ImVec2_c){0, 0}, (ImVec2_c){1, 1});
    }

    // Dummy element to advance the cursor past the preview box
    igSetCursorScreenPos((ImVec2_c){previewPos.x, previewPos.y + UI_PREVIEW_BOX_HEIGHT + UI_SPACING});
}
