#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "configManager.h"
#include "whisperEngine.h"

SDL_Texture *createTextTexture(SDL_Renderer *renderer, TTF_Font *font, SubtitleToken *textToken, int textTokenNum, AppConfig *config, float *text_width, float *text_height) {
    // if (font == NULL || textToken == NULL || textTokenNum <= 0) {
    //     return NULL;
    // } else {
    //     char text[1024] = {};

    //     for(int i = 0;i < textTokenNum;++ i)
    //     {
    //         const char *tmpText = textToken[i].text;
    //         if(tmpText != NULL && strcmp(tmpText,"<|endoftext|>") != 0)
    //         {
    //             strncat(text,tmpText,sizeof(text) - strlen(text) - 1);
    //         }
    //     }

    //     SDL_Color bgColor = config->text_outline_color;
    //     SDL_Color fgColor = config->text_color;

    //     int thickness = config->outline_thickness;
    //     TTF_SetFontOutline(font, thickness); // set thickness
    //     SDL_Surface *backGroundText = TTF_RenderText_Blended(font, text, 0, bgColor);

    //     TTF_SetFontOutline(font, 0); // set thickness
    //     SDL_Surface *foreGroundText = TTF_RenderText_Blended(font, text, 0, fgColor);

    //     SDL_Rect destinationRect = {thickness, thickness, backGroundText->w, backGroundText->h};
    //     SDL_BlitSurface(foreGroundText, NULL, backGroundText, &destinationRect); // combine surfaces into backGrounText
    //     SDL_Surface *surface = backGroundText;
    //     SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    //     *text_width = (float)surface->w;
    //     *text_height = (float)surface->h;

    //     SDL_DestroySurface(foreGroundText);
    //     SDL_DestroySurface(surface); // Clean up surface

    //     return texture;
    // }
    if (font == NULL || textToken == NULL || textTokenNum <= 0) {
        return NULL;
    } else {
        SDL_Color bgColor = config->text_outline_color;
        SDL_Color fgColor = config->text_color;
        int thickness = config->outline_thickness;
        float cursor_x = 0.0f;

        int total_width = 0;
        int max_height = 0;
        const int word_spacing = 2;
        for(int i = 0;i < textTokenNum;++ i)
        {
            const char *tmpText = textToken[i].text;
            if(tmpText == NULL || strcmp(tmpText,"<|endoftext|>") == 0)continue;
            TTF_SetFontOutline(font,thickness);
            SDL_Surface *temp = TTF_RenderText_Blended(font, tmpText, 0, bgColor);
            if(temp)
            {
                total_width += temp->w + word_spacing;
                if(temp->h > max_height)max_height = temp->h;
            }
        }
        if(total_width > 0)total_width -= word_spacing;
        if(max_height == 0)return NULL;

        SDL_Surface *canvas = SDL_CreateSurface(total_width,max_height,SDL_PIXELFORMAT_ABGR8888);
        if(!canvas)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create canvas: %s", SDL_GetError());
            return NULL;
        }
        SDL_PixelFormat form;
        form = canvas->format;
        const SDL_PixelFormatDetails *details = SDL_GetPixelFormatDetails(form);
        SDL_Palette *palette = SDL_GetSurfacePalette(canvas);
        Uint32 color = SDL_MapRGBA(details,palette,0,0,0,0);
        SDL_FillSurfaceRect(canvas,NULL,color);

        for(int i = 0;i < textTokenNum;++ i)
        {
            const char *tmpText = textToken[i].text;
            if(tmpText != NULL && strcmp(tmpText,"<|endoftext|>") != 0)
            {
                TTF_SetFontOutline(font,thickness);
                SDL_Surface *backGroundText = TTF_RenderText_Blended(font, tmpText, 0, bgColor);

                TTF_SetFontOutline(font,0);
                SDL_Surface *foreGroundText = TTF_RenderText_Blended(font, tmpText, 0, fgColor);

                SDL_Surface *tmpSurface = backGroundText;
                SDL_Rect destinationRect = {(int)cursor_x,thickness,tmpSurface->w,tmpSurface->h};
                SDL_BlitSurface(foreGroundText,NULL,tmpSurface,&destinationRect);

                SDL_BlitSurface(tmpSurface,NULL,canvas,&destinationRect);

                cursor_x += tmpSurface->w;
            }
        }

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, canvas);
        *text_width = (float)canvas->w;
        *text_height = (float)canvas->h;

        SDL_DestroySurface(canvas);
        return texture;
    }
}

SDL_Texture *createColorTextTexture(SDL_Renderer *renderer, TTF_Font *font, SubtitleToken *textToken, int textTokenNum, AppConfig *config, float *text_width, float *text_height) {
    if (font == NULL || textToken == NULL || textTokenNum <= 0) {
        return NULL;
    } else {
        SDL_Color bgColor = config->text_outline_color;
        SDL_Color fgColor = config->text_color;
        SDL_Surface *canvas;
        int thickness = config->outline_thickness;
        float cursor_x = 0.0f;

        for(int i = 0;i < textTokenNum;++ i)
        {
            const char *tmpText = textToken[i].text;
            if(tmpText != NULL && strcmp(tmpText,"<|endoftext|>") != 0)
            {
                TTF_SetFontOutline(font,thickness);
                SDL_Surface *backGroundText = TTF_RenderText_Blended(font, tmpText, 0, bgColor);

                TTF_SetFontOutline(font,0);
                SDL_Surface *foreGroundText = TTF_RenderText_Blended(font, tmpText, 0, fgColor);

                SDL_Surface *tmpSurface = backGroundText;
                SDL_Rect destinationRect = {(int)cursor_x,thickness,tmpSurface->w,tmpSurface->h};
                SDL_BlitSurface(foreGroundText,NULL,tmpSurface,&destinationRect);

                SDL_BlitSurface(tmpSurface,NULL,canvas,&destinationRect);

                cursor_x += tmpSurface->w;

                SDL_DestroySurface(tmpSurface);
                SDL_DestroySurface(backGroundText);
                SDL_DestroySurface(foreGroundText);
            }
        }

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, canvas);
        *text_width = (float)canvas->w;
        *text_height = (float)canvas->h;

        SDL_DestroySurface(canvas);
        return texture;
    }
}

SDL_Texture *createOpacityTextTexture(SDL_Renderer *renderer, TTF_Font *font, SubtitleToken *textToken, int textTokenNum, AppConfig *config, float *text_width, float *text_height) {
    if (font == NULL || textToken == NULL || textTokenNum <= 0) {
        return NULL;
    } else {
        // SDL_Color bgColor = config->text_outline_color;
        // SDL_Color fgColor = config->text_color;

        // int thickness = config->outline_thickness;
        // TTF_SetFontOutline(font, thickness); // set thickness
        // SDL_Surface *backGroundText = TTF_RenderText_Blended(font, text, 0, bgColor);

        // TTF_SetFontOutline(font, 0); // set thickness
        // SDL_Surface *foreGroundText = TTF_RenderText_Blended(font, text, 0, fgColor);

        // SDL_Rect destinationRect = {thickness, thickness, backGroundText->w, backGroundText->h};
        // SDL_BlitSurface(foreGroundText, NULL, backGroundText, &destinationRect); // combine surfaces into backGrounText
        // SDL_Surface *surface = backGroundText;
        // SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

        // *text_width = (float)surface->w;
        // *text_height = (float)surface->h;

        // SDL_DestroySurface(foreGroundText);
        // SDL_DestroySurface(surface); // Clean up surface

        // return texture;
    }
}