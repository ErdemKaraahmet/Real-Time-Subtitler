#include "textTexture.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FIXED_CONTAINER_WIDTH 1000
#define MAX_WORD_LEN 64
#define MAX_LINE_WORDS 64
#define MARGIN_X 10
#define SCROLL_ANIM_MS 220

typedef struct {
    char text[MAX_WORD_LEN];
    float probability;
} SubWordToken;

typedef struct {
    char full_text[MAX_WORD_LEN];
    SubWordToken tokens[16];
    int token_count;
    int pixel_width;
} CaptionWord;

typedef struct {
    CaptionWord words[MAX_LINE_WORDS];
    int word_count;
    int total_width;
    SDL_Surface *cachedSurface;
} CaptionLine;

static CaptionLine s_line1 = {0};
static CaptionLine s_line2 = {0};
static CaptionLine s_oldLine1 = {0};
static Uint64 s_scrollAnimStart = 0;
static bool s_scrollAnimActive = false;

static void freeCaptionLine(CaptionLine *line) {
    if (line->cachedSurface != NULL) {
        SDL_DestroySurface(line->cachedSurface);
        line->cachedSurface = NULL;
    }
    memset(line, 0, sizeof(*line));
}

bool isCaptionScrollAnimating(void) {
    if (!s_scrollAnimActive)
        return false;
    Uint64 elapsed = SDL_GetTicks() - s_scrollAnimStart;
    if (elapsed >= SCROLL_ANIM_MS) {
        s_scrollAnimActive = false;
        return false;
    }
    return true;
}

void resetCaptionBuffer(void) {
    freeCaptionLine(&s_line1);
    freeCaptionLine(&s_line2);
    freeCaptionLine(&s_oldLine1);
    s_scrollAnimActive = false;
    s_scrollAnimStart = 0;
}

static int getSpaceWidth(TTF_Font *font, int thickness) {
    int spaceW = 0, h = 0;
    TTF_SetFontOutline(font, thickness);
    if (!TTF_GetStringSize(font, " ", 0, &spaceW, &h)) {
        spaceW = 10;
    }
    TTF_SetFontOutline(font, 0);
    return spaceW > (2 * thickness) ? (spaceW - 2 * thickness) : spaceW;
}

static void measureWordWidth(TTF_Font *font, CaptionWord *word, int thickness) {
    if (!word)
        return;
    if (!font || word->full_text[0] == '\0') {
        word->pixel_width = 0;
        return;
    }
    int w = 0, h = 0;
    TTF_SetFontOutline(font, thickness);
    if (TTF_GetStringSize(font, word->full_text, 0, &w, &h)) {
        word->pixel_width = w > (2 * thickness) ? (w - 2 * thickness) : w;
    } else {
        word->pixel_width = 0;
    }
    TTF_SetFontOutline(font, 0);
}

static void addWordToLine(CaptionLine *line, const CaptionWord *word, int space_width) {
    if (line->word_count < MAX_LINE_WORDS) {
        line->words[line->word_count] = *word;
        if (line->word_count > 0) {
            line->total_width += space_width;
        }
        line->total_width += word->pixel_width;
        line->word_count++;
        if (line->cachedSurface != NULL) {
            SDL_DestroySurface(line->cachedSurface);
            line->cachedSurface = NULL;
        }
    }
}

static void placeWordInLine(const CaptionWord *word, int spaceW, int maxW) {
    if (s_line2.word_count == 0) {
        if (s_line1.word_count == 0 || s_line1.total_width + spaceW + word->pixel_width <= maxW) {
            addWordToLine(&s_line1, word, spaceW);
        } else {
            addWordToLine(&s_line2, word, spaceW);
        }
    } else {
        if (s_line2.total_width + spaceW + word->pixel_width <= maxW) {
            addWordToLine(&s_line2, word, spaceW);
        } else {
            // Discrete Scroll Shift with Animation
            freeCaptionLine(&s_oldLine1);
            s_oldLine1 = s_line1;
            s_line1 = s_line2;
            memset(&s_line2, 0, sizeof(s_line2));
            addWordToLine(&s_line2, word, spaceW);
            s_scrollAnimStart = SDL_GetTicks();
            s_scrollAnimActive = true;
        }
    }
}

static void processIncomingWords(TTF_Font *font, SubtitleToken *tokens, int tokenNum, int thickness) {
    if (!font || !tokens || tokenNum <= 0)
        return;

    int spaceW = getSpaceWidth(font, thickness);
    int max_line_width = FIXED_CONTAINER_WIDTH - (2 * MARGIN_X) - (2 * thickness);

    // Assemble tokens into words
    CaptionWord currentWord;
    memset(&currentWord, 0, sizeof(currentWord));

    for (int i = 0; i < tokenNum; ++i) {
        const char *tText = tokens[i].text;
        if (tText == NULL || tText[0] == '\0' || strcmp(tText, "<|endoftext|>") == 0 || strcmp(tText, "[_EOT_]") == 0) {
            continue;
        }

        bool startsWithSpace = (tText[0] == ' ');
        const char *cleanText = startsWithSpace ? (tText + 1) : tText;

        if (cleanText[0] == '\0') {
            continue;
        }

        if (startsWithSpace && currentWord.full_text[0] != '\0') {
            // Finalize previous word and place it
            measureWordWidth(font, &currentWord, thickness);
            placeWordInLine(&currentWord, spaceW, max_line_width);
            memset(&currentWord, 0, sizeof(currentWord));
        }

        // Append sub-word token to current word
        SDL_strlcat(currentWord.full_text, cleanText, sizeof(currentWord.full_text));
        if (currentWord.token_count < 16) {
            SDL_strlcpy(currentWord.tokens[currentWord.token_count].text, cleanText, sizeof(currentWord.tokens[currentWord.token_count].text));
            currentWord.tokens[currentWord.token_count].probability = tokens[i].probability;
            currentWord.token_count++;
        }
    }

    // Place remaining word if present
    if (currentWord.full_text[0] != '\0') {
        measureWordWidth(font, &currentWord, thickness);
        placeWordInLine(&currentWord, spaceW, max_line_width);
    }
}

static SDL_Surface *renderLinePlainSurface(TTF_Font *font, const CaptionLine *line, const AppConfig *config) {
    if (line->word_count == 0)
        return NULL;

    char lineText[2048] = {0};
    for (int i = 0; i < line->word_count; ++i) {
        if (i > 0) {
            SDL_strlcat(lineText, " ", sizeof(lineText));
        }
        SDL_strlcat(lineText, line->words[i].full_text, sizeof(lineText));
    }

    if (lineText[0] == '\0')
        return NULL;

    int thickness = config->outline_thickness;
    TTF_SetFontOutline(font, thickness);
    SDL_Surface *backGroundText = TTF_RenderText_Blended(font, lineText, 0, config->text_outline_color);
    if (!backGroundText)
        return NULL;

    TTF_SetFontOutline(font, 0);
    SDL_Surface *foreGroundText = TTF_RenderText_Blended(font, lineText, 0, config->text_color);

    if (foreGroundText) {
        SDL_Rect foreRect = {thickness, thickness, foreGroundText->w, foreGroundText->h};
        SDL_BlitSurface(foreGroundText, NULL, backGroundText, &foreRect);
        SDL_DestroySurface(foreGroundText);
    }

    if (config->text_bg_color.a > 0) {
        int padX = 6;
        int padY = 2;

        int rectW = backGroundText->w + 2 * padX;
        int rectH = backGroundText->h + 2 * padY;

        SDL_Surface *bgSurface = SDL_CreateSurface(rectW, rectH, SDL_PIXELFORMAT_ABGR8888);
        if (bgSurface) {
            SDL_PixelFormat format = bgSurface->format;
            const SDL_PixelFormatDetails *details = SDL_GetPixelFormatDetails(format);
            SDL_Palette *palette = SDL_GetSurfacePalette(bgSurface);
            Uint32 clearColor = SDL_MapRGBA(details, palette, 0, 0, 0, 0);
            SDL_FillSurfaceRect(bgSurface, NULL, clearColor);
            Uint32 bgColor =
                SDL_MapRGBA(details, palette, config->text_bg_color.r, config->text_bg_color.g, config->text_bg_color.b, config->text_bg_color.a);
            SDL_Rect bgRect = {0, 0, bgSurface->w, bgSurface->h};
            SDL_FillSurfaceRect(bgSurface, &bgRect, bgColor);

            SDL_Rect textDst = {padX, padY, backGroundText->w, backGroundText->h};
            SDL_BlitSurface(backGroundText, NULL, bgSurface, &textDst);
            SDL_DestroySurface(backGroundText);
            return bgSurface;
        }
    }

    return backGroundText;
}

static void renderLineOpacityToCanvas(TTF_Font *font, const CaptionLine *line, SDL_Surface *canvas, int yOffset, int startX, const AppConfig *config,
                                      int refH) {
    if (!font || !line || !canvas || line->word_count == 0)
        return;

    int thickness = config->outline_thickness;
    int spaceW = getSpaceWidth(font, thickness);

    float cursor_x = (float)startX;

    for (int wIdx = 0; wIdx < line->word_count; ++wIdx) {
        const CaptionWord *word = &line->words[wIdx];
        if (wIdx > 0) {
            cursor_x += (float)spaceW;
        }

        for (int tIdx = 0; tIdx < word->token_count; ++tIdx) {
            const char *tText = word->tokens[tIdx].text;
            if (tText == NULL || tText[0] == '\0')
                continue;

            SDL_Color tokenBgColor = config->text_outline_color;
            SDL_Color tokenFgColor = config->text_color;
            float prob = SDL_clamp(word->tokens[tIdx].probability, 0.0f, 1.0f);
            float alphaFactor = 0.40f + (0.60f * prob);

            tokenBgColor.a = (Uint8)((float)config->text_outline_color.a * alphaFactor);
            tokenFgColor.a = (Uint8)((float)config->text_color.a * alphaFactor);

            TTF_SetFontOutline(font, thickness);
            SDL_Surface *backGroundText = TTF_RenderText_Blended(font, tText, 0, tokenBgColor);
            if (!backGroundText)
                continue;

            TTF_SetFontOutline(font, 0);
            SDL_Surface *foreGroundText = TTF_RenderText_Blended(font, tText, 0, tokenFgColor);

            if (foreGroundText) {
                SDL_Rect foreRect = {thickness, thickness, foreGroundText->w, foreGroundText->h};
                SDL_BlitSurface(foreGroundText, NULL, backGroundText, &foreRect);
                SDL_DestroySurface(foreGroundText);
            }

            int tokenY = yOffset + (refH - backGroundText->h) / 2;
            if (tokenY < yOffset) {
                tokenY = yOffset;
            }

            SDL_Rect canvasRect = {(int)cursor_x, tokenY, backGroundText->w, backGroundText->h};
            SDL_BlitSurface(backGroundText, NULL, canvas, &canvasRect);

            int advance = backGroundText->w > (2 * thickness) ? (backGroundText->w - 2 * thickness) : backGroundText->w;
            cursor_x += (float)advance;

            SDL_DestroySurface(backGroundText);
        }
    }
}

static SDL_Surface *renderLineConfidenceSurface(TTF_Font *font, const CaptionLine *line, const AppConfig *config) {
    if (line->word_count == 0)
        return NULL;

    char lineText[2048] = {0};
    for (int i = 0; i < line->word_count; ++i) {
        if (i > 0) {
            SDL_strlcat(lineText, " ", sizeof(lineText));
        }
        SDL_strlcat(lineText, line->words[i].full_text, sizeof(lineText));
    }
    if (lineText[0] == '\0')
        return NULL;

    int thickness = config->outline_thickness;
    TTF_SetFontOutline(font, thickness);
    SDL_Surface *refSurf = TTF_RenderText_Blended(font, lineText, 0, config->text_outline_color);
    TTF_SetFontOutline(font, 0);

    if (!refSurf)
        return NULL;

    int refW = refSurf->w;
    int refH = refSurf->h;
    SDL_DestroySurface(refSurf);

    int padX = (config->text_bg_color.a > 0) ? 6 : 0;
    int padY = (config->text_bg_color.a > 0) ? 2 : 0;

    int canvasW = refW + 2 * padX;
    int canvasH = refH + 2 * padY;

    SDL_Surface *canvas = SDL_CreateSurface(canvasW, canvasH, SDL_PIXELFORMAT_ABGR8888);
    if (!canvas)
        return NULL;

    SDL_PixelFormat format = canvas->format;
    const SDL_PixelFormatDetails *details = SDL_GetPixelFormatDetails(format);
    SDL_Palette *palette = SDL_GetSurfacePalette(canvas);
    Uint32 clearColor = SDL_MapRGBA(details, palette, 0, 0, 0, 0);
    SDL_FillSurfaceRect(canvas, NULL, clearColor);

    if (config->text_bg_color.a > 0) {
        Uint32 bgColor =
            SDL_MapRGBA(details, palette, config->text_bg_color.r, config->text_bg_color.g, config->text_bg_color.b, config->text_bg_color.a);
        SDL_Rect bgRect = {0, 0, canvas->w, canvas->h};
        SDL_FillSurfaceRect(canvas, &bgRect, bgColor);
    }

    renderLineOpacityToCanvas(font, line, canvas, padY, padX, config, refH);
    return canvas;
}

static SDL_Surface *getOrRenderLineSurface(TTF_Font *font, CaptionLine *line, const AppConfig *config) {
    if (!line || line->word_count == 0)
        return NULL;
    if (line->cachedSurface != NULL)
        return line->cachedSurface;

    if (config->display_mode == 0) {
        line->cachedSurface = renderLinePlainSurface(font, line, config);
    } else {
        line->cachedSurface = renderLineConfidenceSurface(font, line, config);
    }
    return line->cachedSurface;
}

SDL_Texture *createTextTexture(SDL_Renderer *renderer, TTF_Font *font, SubtitleToken *textToken, int textTokenNum, const AppConfig *config,
                               float *text_width, float *text_height, bool is_new_tokens) {
    if (font == NULL || textToken == NULL || textTokenNum <= 0 || config == NULL || !renderer) {
        return NULL;
    }

    int thickness = config->outline_thickness;
    if (is_new_tokens) {
        processIncomingWords(font, textToken, textTokenNum, thickness);
    }

    if (s_line1.word_count == 0 && s_line2.word_count == 0) {
        return NULL;
    }

    int fontH = TTF_GetFontHeight(font);
    int lineH = fontH + (2 * thickness) + 4;
    int canvasH = 2 * lineH;

    SDL_Surface *canvas = SDL_CreateSurface(FIXED_CONTAINER_WIDTH, canvasH, SDL_PIXELFORMAT_ABGR8888);
    if (!canvas) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create canvas surface: %s", SDL_GetError());
        return NULL;
    }

    SDL_PixelFormat format = canvas->format;
    const SDL_PixelFormatDetails *details = SDL_GetPixelFormatDetails(format);
    SDL_Palette *palette = SDL_GetSurfacePalette(canvas);
    Uint32 clearColor = SDL_MapRGBA(details, palette, 0, 0, 0, 0);
    SDL_FillSurfaceRect(canvas, NULL, clearColor);

    int y_old1 = -lineH;
    int y_line1 = 0;
    int y_line2 = lineH;

    if (s_scrollAnimActive) {
        Uint64 elapsed = SDL_GetTicks() - s_scrollAnimStart;
        if (elapsed < SCROLL_ANIM_MS) {
            float t = (float)elapsed / (float)SCROLL_ANIM_MS;
            float invT = 1.0f - t;
            float eased = 1.0f - (invT * invT * invT);
            y_old1 = (int)(-eased * (float)lineH);
            y_line1 = (int)((1.0f - eased) * (float)lineH);
            y_line2 = (int)((2.0f - eased) * (float)lineH);
        } else {
            s_scrollAnimActive = false;
        }
    }

    if (s_scrollAnimActive && s_oldLine1.word_count > 0) {
        SDL_Surface *sOld = getOrRenderLineSurface(font, &s_oldLine1, config);
        if (sOld) {
            SDL_Rect rOld = {MARGIN_X, y_old1, sOld->w, sOld->h};
            SDL_BlitSurface(sOld, NULL, canvas, &rOld);
        }
    }

    if (s_line1.word_count > 0) {
        SDL_Surface *s1 = getOrRenderLineSurface(font, &s_line1, config);
        if (s1) {
            SDL_Rect r1 = {MARGIN_X, y_line1, s1->w, s1->h};
            SDL_BlitSurface(s1, NULL, canvas, &r1);
        }
    }

    if (s_line2.word_count > 0) {
        SDL_Surface *s2 = getOrRenderLineSurface(font, &s_line2, config);
        if (s2) {
            SDL_Rect r2 = {MARGIN_X, y_line2, s2->w, s2->h};
            SDL_BlitSurface(s2, NULL, canvas, &r2);
        }
    }

    SDL_Texture *resultTexture = SDL_CreateTextureFromSurface(renderer, canvas);
    if (resultTexture != NULL) {
        *text_width = (float)canvas->w;
        *text_height = (float)canvas->h;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture from surface: %s", SDL_GetError());
    }

    SDL_DestroySurface(canvas);
    return resultTexture;
}

static void buildCaptionLineFromTokens(TTF_Font *font, SubtitleToken *tokens, int tokenNum, int thickness, CaptionLine *outLine) {
    int spaceW = getSpaceWidth(font, thickness);
    CaptionWord currentWord;
    memset(&currentWord, 0, sizeof(currentWord));

    for (int i = 0; i < tokenNum; ++i) {
        const char *tText = tokens[i].text;
        if (tText == NULL || tText[0] == '\0' || strcmp(tText, "<|endoftext|>") == 0 || strcmp(tText, "[_EOT_]") == 0) {
            continue;
        }

        bool startsWithSpace = (tText[0] == ' ');
        const char *cleanText = startsWithSpace ? (tText + 1) : tText;
        if (cleanText[0] == '\0')
            continue;

        if (startsWithSpace && currentWord.full_text[0] != '\0') {
            measureWordWidth(font, &currentWord, thickness);
            addWordToLine(outLine, &currentWord, spaceW);
            memset(&currentWord, 0, sizeof(currentWord));
        }

        SDL_strlcat(currentWord.full_text, cleanText, sizeof(currentWord.full_text));
        if (currentWord.token_count < 16) {
            SDL_strlcpy(currentWord.tokens[currentWord.token_count].text, cleanText, sizeof(currentWord.tokens[currentWord.token_count].text));
            currentWord.tokens[currentWord.token_count].probability = tokens[i].probability;
            currentWord.token_count++;
        }
    }
    if (currentWord.full_text[0] != '\0') {
        measureWordWidth(font, &currentWord, thickness);
        addWordToLine(outLine, &currentWord, spaceW);
    }
}

SDL_Texture *createPreviewTextTexture(SDL_Renderer *renderer, TTF_Font *font, SubtitleToken *textToken, int textTokenNum, const AppConfig *config,
                                      float *text_width, float *text_height) {
    if (font == NULL || textToken == NULL || textTokenNum <= 0 || config == NULL || !renderer) {
        return NULL;
    }

    int thickness = config->outline_thickness;
    CaptionLine localLine;
    memset(&localLine, 0, sizeof(localLine));

    buildCaptionLineFromTokens(font, textToken, textTokenNum, thickness, &localLine);

    if (localLine.word_count == 0) {
        return NULL;
    }

    SDL_Surface *canvas = NULL;
    if (config->display_mode == 0) {
        canvas = renderLinePlainSurface(font, &localLine, config);
    } else {
        canvas = renderLineConfidenceSurface(font, &localLine, config);
    }

    if (!canvas) {
        freeCaptionLine(&localLine);
        return NULL;
    }

    SDL_Texture *resultTexture = SDL_CreateTextureFromSurface(renderer, canvas);
    if (resultTexture != NULL) {
        *text_width = (float)canvas->w;
        *text_height = (float)canvas->h;
    }

    freeCaptionLine(&localLine);
    SDL_DestroySurface(canvas);
    return resultTexture;
}
