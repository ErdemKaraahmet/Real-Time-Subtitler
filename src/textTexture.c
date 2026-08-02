#include "textTexture.h"
#include <string.h>

static SDL_Surface *renderPlainTextSurface(TTF_Font *font, SubtitleToken *textToken, int textTokenNum, const AppConfig *config) {
    char fullText[2048] = {0};
    for (int i = 0; i < textTokenNum; ++i) {
        const char *tmpText = textToken[i].text;
        if (tmpText == NULL || tmpText[0] == '\0' || strcmp(tmpText, "<|endoftext|>") == 0) {
            continue;
        }
        SDL_strlcat(fullText, tmpText, sizeof(fullText));
    }

    if (fullText[0] == '\0') {
        return NULL;
    }

    int thickness = config->outline_thickness;
    TTF_SetFontOutline(font, thickness);
    SDL_Surface *backGroundText = TTF_RenderText_Blended(font, fullText, 0, config->text_outline_color);
    if (!backGroundText) {
        return NULL;
    }

    TTF_SetFontOutline(font, 0);
    SDL_Surface *foreGroundText = TTF_RenderText_Blended(font, fullText, 0, config->text_color);

    if (foreGroundText) {
        SDL_Rect foreRect = {thickness, thickness, foreGroundText->w, foreGroundText->h};
        SDL_BlitSurface(foreGroundText, NULL, backGroundText, &foreRect);
        SDL_DestroySurface(foreGroundText);
    }

    return backGroundText;
}

static SDL_Surface *renderOpacityTokensSurface(TTF_Font *font, SubtitleToken *textToken, int textTokenNum, const AppConfig *config) {
    int thickness = config->outline_thickness;
    int total_width = 0;
    int max_height = 0;

    for (int i = 0; i < textTokenNum; ++i) {
        const char *tmpText = textToken[i].text;
        if (tmpText == NULL || tmpText[0] == '\0' || strcmp(tmpText, "<|endoftext|>") == 0) {
            continue;
        }
        int w = 0, h = 0;
        TTF_SetFontOutline(font, thickness);
        if (TTF_GetStringSize(font, tmpText, 0, &w, &h)) {
            int tokenWidth = w > (2 * thickness) ? (w - 2 * thickness) : w;
            total_width += tokenWidth;
            if (h > max_height) {
                max_height = h;
            }
        }
    }

    if (total_width <= 0 || max_height <= 0) {
        return NULL;
    }

    total_width += 2 * thickness;

    SDL_Surface *canvas = SDL_CreateSurface(total_width, max_height, SDL_PIXELFORMAT_ABGR8888);
    if (!canvas) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create canvas surface: %s", SDL_GetError());
        return NULL;
    }

    SDL_PixelFormat format = canvas->format;
    const SDL_PixelFormatDetails *details = SDL_GetPixelFormatDetails(format);
    SDL_Palette *palette = SDL_GetSurfacePalette(canvas);
    Uint32 clearColor = SDL_MapRGBA(details, palette, 0, 0, 0, 0);
    SDL_FillSurfaceRect(canvas, NULL, clearColor);

    float cursor_x = 0.0f;
    for (int i = 0; i < textTokenNum; ++i) {
        const char *tmpText = textToken[i].text;
        if (tmpText == NULL || tmpText[0] == '\0' || strcmp(tmpText, "<|endoftext|>") == 0) {
            continue;
        }

        SDL_Color tokenBgColor = config->text_outline_color;
        SDL_Color tokenFgColor = config->text_color;

        float prob = SDL_clamp(textToken[i].probability, 0.0f, 1.0f);

        // Map probability [0.0, 1.0] to opacity range [0.40, 1.0]
        float alphaFactor = 0.40f + (0.60f * prob);

        tokenBgColor.a = (Uint8)((float)config->text_outline_color.a * alphaFactor);
        tokenFgColor.a = (Uint8)((float)config->text_color.a * alphaFactor);

        TTF_SetFontOutline(font, thickness);
        SDL_Surface *backGroundText = TTF_RenderText_Blended(font, tmpText, 0, tokenBgColor);
        if (!backGroundText) {
            continue;
        }

        TTF_SetFontOutline(font, 0);
        SDL_Surface *foreGroundText = TTF_RenderText_Blended(font, tmpText, 0, tokenFgColor);

        if (foreGroundText) {
            SDL_Rect foreRect = {thickness, thickness, foreGroundText->w, foreGroundText->h};
            SDL_BlitSurface(foreGroundText, NULL, backGroundText, &foreRect);
            SDL_DestroySurface(foreGroundText);
        }

        SDL_Rect canvasRect = {(int)cursor_x, 0, backGroundText->w, backGroundText->h};
        SDL_BlitSurface(backGroundText, NULL, canvas, &canvasRect);

        int advance = backGroundText->w > (2 * thickness) ? (backGroundText->w - 2 * thickness) : backGroundText->w;
        cursor_x += (float)advance;

        SDL_DestroySurface(backGroundText);
    }

    return canvas;
}

SDL_Texture *createTextTexture(SDL_Renderer *renderer, TTF_Font *font, SubtitleToken *textToken, int textTokenNum, const AppConfig *config,
                               float *text_width, float *text_height) {
    if (font == NULL || textToken == NULL || textTokenNum <= 0 || config == NULL) {
        return NULL;
    }

    SDL_Surface *textSurface = NULL;
    if (config->display_mode == 0) {
        textSurface = renderPlainTextSurface(font, textToken, textTokenNum, config);
    } else if (config->display_mode == 1) {
        textSurface = renderOpacityTokensSurface(font, textToken, textTokenNum, config);
    } else {
        return NULL;
    }

    if (textSurface == NULL) {
        return NULL;
    }

    SDL_Texture *resultTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (resultTexture != NULL) {
        *text_width = (float)textSurface->w;
        *text_height = (float)textSurface->h;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture from surface: %s", SDL_GetError());
    }

    SDL_DestroySurface(textSurface);
    return resultTexture;
}
