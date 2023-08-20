//system
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//onion
#include "cjson/cJSON.h"
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

//local
#include "cache_local.h"
#include "misc_utils.h"

int cache_count = 0;

// File cache
cJSON* cacheReadFromFile() {
    FILE* file = fopen(LOCAL_CACHE, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = malloc(length);
    fread(data, 1, length, file);
    fclose(file);

    cJSON* json = cJSON_Parse(data);
    free(data);

    return json;
}

void cacheWriteToFile(cJSON* json) {
    FILE* file = fopen(LOCAL_CACHE, "wb");
    if (!file) return;

    char* data = cJSON_Print(json);
    fwrite(data, 1, strlen(data), file);

    free(data);
    fclose(file);
}

char* cacheLookupRomLocal(const char* gameName) {
    miscLogOutput("romCache: Looking up local ROM cache for game: %s", gameName);
    cJSON* json = cacheReadFromFile();
    if (json) {
        cJSON* entry = NULL;
        cJSON_ArrayForEach(entry, json) {
            if (strcmp(cJSON_GetObjectItem(entry, "gameName")->valuestring, gameName) == 0) {
                char* romPath = cJSON_GetObjectItem(entry, "romPath")->valuestring;
                if (romPath[0] != '\0') {
                    miscLogOutput("romCache: Found ROM in local cache: %s", romPath);
                    char* returnPath = strdup(romPath); 
                    cJSON_Delete(json);
                    return returnPath;
                } else {
                    miscLogOutput("romCache: Entry found in local cache but ROM path is empty.");
                    cJSON_Delete(json);
                    return NULL;
                }
            }
        }
        cJSON_Delete(json);
    }
    miscLogOutput("Game not found in local cache.");
    return NULL;
}


void cacheAddRom(const char* gameName, const char* romPath) {
    miscLogOutput("romCache: Adding ROM to local cache: %s -> %s", gameName, romPath);
    cJSON* json = cacheReadFromFile();
    if (!json) json = cJSON_CreateArray();

    if (cJSON_GetArraySize(json) < MAX_CACHE_ENTRIES) {
        cJSON* entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "gameName", gameName);
        cJSON_AddStringToObject(entry, "romPath", romPath);
        cJSON_AddItemToArray(json, entry);

        cacheWriteToFile(json);
    } else {
        miscLogOutput("romCache: Warning: Local cache is full! ROM not added.");
    }
    cJSON_Delete(json);
}


// image cache

SDL_Surface* cacheGetImage(const char* img_path) {
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(img_cache[i].path, img_path) == 0) {
            miscLogOutput("imageCache: Fetching image from cache: %s", img_path);
            return img_cache[i].surface;
        }
    }

    SDL_Surface* newImage = IMG_Load(img_path);
    if (newImage) {
        if (cache_count < MAX_CACHE_SIZE) {
            img_cache[cache_count].path = img_path;
            img_cache[cache_count].surface = newImage;
            cache_count++;
            miscLogOutput("imageCache: Image loaded and added to cache: %s", img_path);
        } 
        else {
            miscLogOutput("imageCache: Cache full! Image loaded but not added to cache: %s", img_path);
        }
    } 
    else {
        miscLogOutput("imageCache: Failed to load image: %s. SDL_image error: %s", img_path, IMG_GetError());
    }

    return newImage;
}

void cacheClearImageCache() {
    for (int i = 0; i < cache_count; i++) {
        SDL_FreeSurface(img_cache[i].surface);
        img_cache[i].path = NULL;
    }
    miscLogOutput("Cache cleared. Total images removed: %d", cache_count);
    cache_count = 0;
}