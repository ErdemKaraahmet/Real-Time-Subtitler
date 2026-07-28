#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "configManager.h"
#include "whisperEngine.h"

SDL_Texture *createTextTexture(SDL_Renderer *renderer, TTF_Font *font, SubtitleToken *textToken, int textTokenNum, AppConfig *config, float *text_width, float *text_height) 
{
    
}

SDL_Texture *createNormalTextTexture(SDL_Renderer *renderer, TTF_Font *font, SubtitleToken *textToken, int textTokenNum, AppConfig *config, float *text_width, float *text_height) {
    if (font == NULL || textToken == NULL || textTokenNum <= 0) {
        return NULL;
    } else {
        SDL_Color bgColor = config->text_outline_color;
        SDL_Color fgColor = config->text_color;
        int thickness = config->outline_thickness;
        float cursor_x = 0.0f;

        int total_width = 0;
        int max_height = 0;
        for(int i = 0;i < textTokenNum;++ i)
        {
            const char *tmpText = textToken[i].text;
            if(tmpText == NULL || strcmp(tmpText,"<|endoftext|>") == 0)continue;
            TTF_SetFontOutline(font,thickness);
            SDL_Surface *temp = TTF_RenderText_Blended(font, tmpText, 0, bgColor);
            if(temp)
            {
                total_width += temp->w;
                if(temp->h > max_height)max_height = temp->h;
            }
            SDL_DestroySurface(temp);
        }
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
                SDL_Rect foreRect = {thickness,thickness,foreGroundText->w,foreGroundText->h};
                SDL_Rect canvaRect = {(int)cursor_x,0,tmpSurface->w,tmpSurface->h};
                SDL_BlitSurface(foreGroundText,NULL,tmpSurface,&foreRect);

                SDL_BlitSurface(tmpSurface,NULL,canvas,&canvaRect);

                cursor_x += (tmpSurface->w + 2);
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
        int thickness = config->outline_thickness;
        float cursor_x = 0.0f;

        int total_width = 0;
        int max_height = 0;;
        for(int i = 0;i < textTokenNum;++ i)
        {
            const char *tmpText = textToken[i].text;
            if(tmpText == NULL || strcmp(tmpText,"<|endoftext|>") == 0)continue;
            TTF_SetFontOutline(font,thickness);
            SDL_Surface *temp = TTF_RenderText_Blended(font, tmpText, 0, bgColor);
            if(temp)
            {
                total_width += temp->w;
                if(temp->h > max_height)max_height = temp->h;
            }
            SDL_DestroySurface(temp);
        }
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
                SDL_Rect foreRect = {thickness,thickness,foreGroundText->w,foreGroundText->h};
                SDL_Rect canvaRect = {(int)cursor_x,0,tmpSurface->w,tmpSurface->h};
                SDL_BlitSurface(foreGroundText,NULL,tmpSurface,&foreRect);

                SDL_BlitSurface(tmpSurface,NULL,canvas,&canvaRect);

                cursor_x += (tmpSurface->w + 2);
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
        SDL_Color bgColor = config->text_outline_color;
        SDL_Color fgColor = config->text_color;
        int thickness = config->outline_thickness;
        float cursor_x = 0.0f;

        int total_width = 0;
        int max_height = 0;
        for(int i = 0;i < textTokenNum;++ i)
        {
            const char *tmpText = textToken[i].text;
            if(tmpText == NULL || strcmp(tmpText,"<|endoftext|>") == 0)continue;
            TTF_SetFontOutline(font,thickness);
            SDL_Surface *temp = TTF_RenderText_Blended(font, tmpText, 0, bgColor);
            if(temp)
            {
                total_width += temp->w;
                if(temp->h > max_height)max_height = temp->h;
            }
            SDL_DestroySurface(temp);
        }
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
                SDL_Rect foreRect = {thickness,thickness,foreGroundText->w,foreGroundText->h};
                SDL_Rect canvaRect = {(int)cursor_x,0,tmpSurface->w,tmpSurface->h};
                SDL_BlitSurface(foreGroundText,NULL,tmpSurface,&foreRect);

                SDL_BlitSurface(tmpSurface,NULL,canvas,&canvaRect);

                cursor_x += (tmpSurface->w + 2);
            }
        }

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, canvas);
        *text_width = (float)canvas->w;
        *text_height = (float)canvas->h;

        SDL_DestroySurface(canvas);
        return texture;
    }
}