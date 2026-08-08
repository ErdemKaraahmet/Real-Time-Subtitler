#include "controlPanel_internal.h"

static void SDLCALL openFontDialogCallback(void *userdata, const char *const *filelist, int filter) {
    (void)userdata;
    (void)filter;
    if (filelist && filelist[0] && filelist[0][0] != '\0') {
        char errBuf[256] = "";
        if (importFontFile(filelist[0], errBuf, sizeof(errBuf))) {
            previewNeedsUpdate = true;
        } else {
            triggerGlobalError("Failed to import font: %s", errBuf);
        }
    } else if (!filelist) {
        const char *sdlErr = SDL_GetError();
        if (sdlErr && sdlErr[0] != '\0') {
            triggerGlobalError("File dialog error: %s", sdlErr);
        }
    }
}

static void sdlColorToFloats(SDL_Color c, float out[3]) {
    out[0] = (float)c.r / 255.0f;
    out[1] = (float)c.g / 255.0f;
    out[2] = (float)c.b / 255.0f;
}

static SDL_Color floatsToSdlColor(const float in[3]) {
    return (SDL_Color){(uint8_t)(in[0] * 255.0f), (uint8_t)(in[1] * 255.0f), (uint8_t)(in[2] * 255.0f), 255};
}

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
    char fontPath[512];
    utilsResolvePath(fontPath, sizeof(fontPath), uiConfig.font);
    TTF_Font *font = TTF_OpenFont(fontPath, (float)uiConfig.font_size);
    if (!font) {
        if (scannedFontCount > 0) {
            char fallbackFont[512];
            (void)snprintf(fallbackFont, sizeof(fallbackFont), "fonts/%s", scannedFonts[0]);
            char fallbackPath[512];
            utilsResolvePath(fallbackPath, sizeof(fallbackPath), fallbackFont);
            font = TTF_OpenFont(fallbackPath, (float)uiConfig.font_size);
            if (font) {
                SDL_strlcpy(uiConfig.font, fallbackFont, sizeof(uiConfig.font));
            }
        }
        if (!font) {
            previewFontLoadFailed = true;
            return;
        }
    }

    // Render preview texture
    previewTexture = createPreviewTextTexture(cpRenderer, font, sample, 3, &uiConfig, &previewWidth, &previewHeight);
    TTF_CloseFont(font);
}

void renderViewPage(void) {
    // Font Selection
    const char *fontDisplayName = getFilenameFromPath(uiConfig.font);
    const char *activeFontFilename = getFilenameFromPath(savedConfig.font);
    igAlignTextToFramePadding();
    igText("Font");
    igSameLine(180.0f, 0.0f);
    igSetNextItemWidth(-1.0f);
    if (igBeginCombo("##Font", fontDisplayName, 0)) {
        if (igSelectable_Bool("Add Font", false, 0, (ImVec2_c){0.0f, 24.0f})) {
            static const SDL_DialogFileFilter filters[] = {{"Font Files (*.ttf, *.otf)", "ttf;otf"}, {"All Files (*)", "*"}};
            SDL_ShowOpenFileDialog(openFontDialogCallback, NULL, cpWindow, filters, 2, NULL, false);
        }

        igSeparator();

        if (scannedFontCount == 0) {
            igSelectable_Bool("No item found in folder##empty_font", false, ImGuiSelectableFlags_Disabled, (ImVec2_c){0, 0});
        } else {
            for (int i = 0; i < scannedFontCount; i++) {
                bool isSelected = (strcmp(fontDisplayName, scannedFonts[i]) == 0);
                bool isActive = (strcmp(activeFontFilename, scannedFonts[i]) == 0);
                char itemDisplay[384];
                (void)SDL_snprintf(itemDisplay, sizeof(itemDisplay), "%s##font%d", scannedFonts[i], i);

                igPushID_Int(i);
                bool rowClicked = igSelectable_Bool(itemDisplay, isSelected, ImGuiSelectableFlags_NoAutoClosePopups, (ImVec2_c){0.0f, 24.0f});

                ImVec2_c minVal = igGetItemRectMin();
                ImVec2_c maxVal = igGetItemRectMax();
                ImDrawList *drawList = igGetWindowDrawList();

                const char *iconStr = isActive ? "[A]" : "[D]";
                ImU32 iconCol =
                    isActive ? igGetColorU32_Vec4((ImVec4_c){0.3f, 1.0f, 0.3f, 1.0f}) : igGetColorU32_Vec4((ImVec4_c){1.0f, 0.3f, 0.3f, 1.0f});

                float iconWidth = igCalcTextSize(iconStr, NULL, false, -1.0f).x;
                float iconX = maxVal.x - iconWidth - 8.0f;
                float iconY = minVal.y + 7.0f;
                ImVec2_c iconPos = {iconX, iconY};
                ImDrawList_AddText_Vec2(drawList, iconPos, iconCol, iconStr, NULL);

                if (rowClicked) {
                    ImVec2_c mousePos = igGetMousePos();
                    bool clickedIcon = (mousePos.x >= maxVal.x - 36.0f);
                    if (clickedIcon && !isActive) {
                        SDL_strlcpy(g_DeleteTargetFontFilename, scannedFonts[i], sizeof(g_DeleteTargetFontFilename));
                    } else if (!clickedIcon) {
                        (void)SDL_snprintf(uiConfig.font, sizeof(uiConfig.font), "fonts/%s", scannedFonts[i]);
                        previewNeedsUpdate = true;
                        igCloseCurrentPopup();
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
    float textColor[3];
    sdlColorToFloats(uiConfig.text_color, textColor);
    igAlignTextToFramePadding();
    igText("Text Color");
    igSameLine(180.0f, 0.0f);
    igSetNextItemWidth(-1.0f);
    if (igColorEdit3("##Text Color", textColor, 0)) {
        uiConfig.text_color = floatsToSdlColor(textColor);
        previewNeedsUpdate = true;
    }

    float outlineColor[3];
    sdlColorToFloats(uiConfig.text_outline_color, outlineColor);
    igAlignTextToFramePadding();
    igText("Outline Color");
    igSameLine(180.0f, 0.0f);
    igSetNextItemWidth(-1.0f);
    if (igColorEdit3("##Outline Color", outlineColor, 0)) {
        uiConfig.text_outline_color = floatsToSdlColor(outlineColor);
        previewNeedsUpdate = true;
    }

    float bgColor[3];
    sdlColorToFloats(uiConfig.text_bg_color, bgColor);
    int bgOpacityPercent = (int)SDL_roundf((float)uiConfig.text_bg_color.a * 100.0f / 255.0f);
    igAlignTextToFramePadding();
    igText("Text Background");
    igSameLine(180.0f, 0.0f);

    float availWidth = igGetContentRegionAvail().x;
    float opacityWidth = 65.0f;
    float colorEditWidth = availWidth - opacityWidth - 8.0f;
    if (colorEditWidth < 80.0f) {
        colorEditWidth = 80.0f;
    }

    igSetNextItemWidth(colorEditWidth);
    if (igColorEdit3("##Text Background", bgColor, 0)) {
        SDL_Color newColor = floatsToSdlColor(bgColor);
        newColor.a = uiConfig.text_bg_color.a;
        uiConfig.text_bg_color = newColor;
        previewNeedsUpdate = true;
    }
    igSameLine(0.0f, 8.0f);
    igSetNextItemWidth(opacityWidth);
    if (igDragInt("##Background Opacity", &bgOpacityPercent, 0.5f, 0, 100, "%d", 0)) {
        int clampedPercent = SDL_clamp(bgOpacityPercent, 0, 100);
        uiConfig.text_bg_color.a = (uint8_t)SDL_roundf((float)clampedPercent * 255.0f / 100.0f);
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
    ImVec2_c previewPos = igGetCursorScreenPos();

    // Draw a dark background rectangle for the preview (no rounding)
    ImDrawList *drawList = igGetWindowDrawList();
    ImDrawList_AddRectFilled(drawList, previewPos, (ImVec2_c){previewPos.x + UI_PREVIEW_BOX_WIDTH, previewPos.y + UI_PREVIEW_BOX_HEIGHT},
                             IM_COL32(15, 15, 15, 255), 0.0f, 0);

    ImDrawList_AddRect(drawList, previewPos, (ImVec2_c){previewPos.x + UI_PREVIEW_BOX_WIDTH, previewPos.y + UI_PREVIEW_BOX_HEIGHT},
                       IM_COL32(80, 80, 80, 255), 0.0f, 1.0f, 0);

    // Render the texture inside the box at 1:1 scale, clipped to container borders
    if (previewFontLoadFailed) {
        igSetCursorScreenPos((ImVec2_c){previewPos.x + 10.0f, previewPos.y + 10.0f});
        igTextColored((ImVec4_c){1.0f, 0.3f, 0.3f, 1.0f}, "Preview unavailable (No valid fonts found)");
    } else if (previewTexture) {
        float displayW = previewWidth;
        float displayH = previewHeight;
        float boxW = UI_PREVIEW_BOX_WIDTH - 4.0f;
        float boxH = UI_PREVIEW_BOX_HEIGHT - 4.0f;

        // Center the 1:1 preview texture inside the box, inset 2px from border lines
        float startX = previewPos.x + 2.0f + (boxW - displayW) / 2.0f;
        float startY = previewPos.y + 2.0f + (boxH - displayH) / 2.0f;

        ImVec2_c clipMin = {previewPos.x + 2.0f, previewPos.y + 2.0f};
        ImVec2_c clipMax = {previewPos.x + UI_PREVIEW_BOX_WIDTH - 2.0f, previewPos.y + UI_PREVIEW_BOX_HEIGHT - 2.0f};

        ImDrawList_PushClipRect(drawList, clipMin, clipMax, true);
        igSetCursorScreenPos((ImVec2_c){startX, startY});
        ImTextureRef_c texRef = {NULL, (ImTextureID)(intptr_t)previewTexture};
        igImage(texRef, (ImVec2_c){displayW, displayH}, (ImVec2_c){0, 0}, (ImVec2_c){1, 1});
        ImDrawList_PopClipRect(drawList);
    }

    // Dummy element to advance the cursor past the preview box
    igSetCursorScreenPos((ImVec2_c){previewPos.x, previewPos.y + UI_PREVIEW_BOX_HEIGHT + UI_SPACING});
}
