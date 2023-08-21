#ifndef CACHE_LOCAL_H
#define CACHE_LOCAL_H

#include "gui_draw.h"
#include "cjson/cJSON.h"

#define LOCAL_CACHE "/mnt/SDCARD/.tmp_update/config/browserCache.json"
#define MAX_CACHE_ENTRIES 10000 // file
#define MAX_IMG_CACHE_SIZE 150 // image

// File
typedef struct {
    char gameName[STR_MAX];
    char romPath[1024];
    char coreName[STR_MAX];
    char corePath[1024];
} CacheEntry;

CacheEntry cache[MAX_CACHE_ENTRIES];

cJSON* cacheReadFromFile();
void cacheWriteToFile(cJSON* json);
char* cacheLookupRomLocal(const char* identifier, bool isCRC);
void cacheAddRom(const char* gameName, const char* romPath, const char* crc);

// image
typedef struct ImageCache {
    const char* path;
    SDL_Surface* surface;
} ImageCache;

ImageCache img_cache[MAX_IMG_CACHE_SIZE];
SDL_Surface* cacheGetImage(const char* img_path);
void cacheClearImageCache();


#endif // CACHE_LOCAL_H
