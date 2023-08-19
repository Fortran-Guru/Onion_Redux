#ifndef CACHE_LOCAL_H
#define CACHE_LOCAL_H

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include "cjson/cJSON.h"

#define LOCAL_CACHE "/mnt/SDCARD/.tmp_update/config/browserCache.json"
#define MAX_CACHE_ENTRIES 10000 // file
#define MAX_CACHE_SIZE 150 // image

// File
typedef struct {
    char gameName[256];
    char romPath[1024];
    char coreName[256];
    char corePath[1024];
} CacheEntry;

CacheEntry cache[MAX_CACHE_ENTRIES];

cJSON* readCacheFromFile();
void writeCacheToFile(cJSON* json);
char* lookupRomCacheLocal(const char* gameName);
void addRomToCacheLocal(const char* gameName, const char* romPath);


// image
typedef struct ImageCache {
    const char* path;
    SDL_Surface* surface;
} ImageCache;

ImageCache img_cache[MAX_CACHE_SIZE];
SDL_Surface* getCachedImage(const char* img_path);
void clearImageCache();


#endif // CACHE_LOCAL_H
