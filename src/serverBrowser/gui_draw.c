//onion
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

//local
#include "gui_draw.h"
#include "myriad_core.h"
#include "misc_utils.h"
#include "cache_local.h"

void drawboxArt(SDL_Surface* screen, const char* img_path) { // draws the boxart
    if (!img_path) {
        log_output("Image path provided is NULL");
        return;
    }
    SDL_Surface* originalImage = getCachedImage(img_path);
    if (!originalImage) {
        log_output("Failed to load image: %s. SDL_image error: %s", img_path, IMG_GetError());
        return;
    }

    SDL_Rect srcCropRect;
    srcCropRect.w = (originalImage->w > 200 ? 200 : originalImage->w);
    srcCropRect.h = (originalImage->h > 150 ? 150 : originalImage->h);

    if (srcCropRect.w < 0 || srcCropRect.h < 0) {
        log_output("Invalid crop dimensions for image: %s", img_path);
        return;
    }

    srcCropRect.x = originalImage->w - srcCropRect.w;
    srcCropRect.y = originalImage->h - srcCropRect.h;

    SDL_Rect destBlitRect;
    destBlitRect.x = screen->w - srcCropRect.w - 425;
    destBlitRect.y = 80;

    if (SDL_BlitSurface(originalImage, &srcCropRect, screen, &destBlitRect) != 0) {
        log_output("Failed to blit image onto screen. SDL error: %s", SDL_GetError());
        SDL_FreeSurface(originalImage);
        return;
    }

    if (SDL_Flip(screen) == -1) {
        log_output("Failed to flip the screen. SDL error: %s", SDL_GetError());
        return;
    }
}

void drawgenericIcon(SDL_Surface* screen, const char* img_path, int x, int y) { // draws something wherever you want
    if (!screen || !img_path) {
        log_output("Invalid parameters provided to drawGenericIcon function.");
        return;
    }

    SDL_Surface* iconImage = getCachedImage(img_path);
    if (!iconImage) {
        log_output("Failed to load image: %s. SDL_image error: %s", img_path, IMG_GetError());
        return;
    }

    SDL_Rect destBlitRect;
    destBlitRect.x = x;
    destBlitRect.y = y;

    if (SDL_BlitSurface(iconImage, NULL, screen, &destBlitRect) != 0) {
        log_output("Failed to blit image onto screen. SDL error: %s", SDL_GetError());
        SDL_FreeSurface(iconImage);
        return;
    }
    
}



