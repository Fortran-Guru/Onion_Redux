//system
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//onion
#include "utils/str.h"

//local
#include "misc_utils.h"
#include "cache_local.h"

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

char* cacheLookupRomLocal(const char* identifier, bool isCRC) {
    miscLogOutput(__func__, "Looking up local ROM cache for %s: %s", isCRC ? "CRC" : "game", identifier);
    cJSON* json = cacheReadFromFile();
    if (json) {
        cJSON* entry = NULL;
        cJSON_ArrayForEach(entry, json) {
            if ((isCRC && strcmp(cJSON_GetObjectItem(entry, "CRC")->valuestring, identifier) == 0) ||
                (!isCRC && strcmp(cJSON_GetObjectItem(entry, "gameName")->valuestring, identifier) == 0)) {
                char* romPath = cJSON_GetObjectItem(entry, "romPath")->valuestring;
                if (romPath[0] != '\0') {
                    miscLogOutput(__func__, "Found ROM in local cache: %s", romPath);
                    char* returnPath = strdup(romPath); 
                    cJSON_Delete(json);
                    return returnPath;
                } else {
                    miscLogOutput(__func__, "Entry found in local cache but ROM path is empty.");
                    cJSON_Delete(json);
                    return NULL;
                }
            }
        }
        cJSON_Delete(json);
    }
    miscLogOutput(__func__, "%s not found in local cache.", isCRC ? "CRC" : "Game");
    return NULL;
}


void cacheAddRom(const char* gameName, const char* romPath, const char* crc) {
    miscLogOutput(__func__, "Adding ROM to local cache: %s -> %s with crc %s", gameName, romPath, crc);
    cJSON* json = cacheReadFromFile();
    if (!json) json = cJSON_CreateArray();

    if (cJSON_GetArraySize(json) < MAX_CACHE_ENTRIES) {
        cJSON* entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "gameName", gameName);
        cJSON_AddStringToObject(entry, "romPath", romPath);
        cJSON_AddStringToObject(entry, "CRC", crc); // Added CRC field
        cJSON_AddItemToArray(json, entry);

        cacheWriteToFile(json);
    } else {
        miscLogOutput(__func__, "Warning: Local cache is full! ROM not added.");
    }
    cJSON_Delete(json);
}

// image cache

SDL_Surface* cacheGetImage(const char* img_path) {
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(img_cache[i].path, img_path) == 0) {
            miscLogOutput(__func__, "Fetching image from cache: %s", img_path);
            return img_cache[i].surface;
        }
    }

    SDL_Surface* newImage = IMG_Load(img_path);
    if (newImage) {
        if (cache_count < MAX_IMG_CACHE_SIZE) {
            img_cache[cache_count].path = img_path;
            img_cache[cache_count].surface = newImage;
            cache_count++;
            miscLogOutput(__func__, "Image loaded and added to cache: %s", img_path);
        } 
        else {
            miscLogOutput(__func__, "Cache full! Image loaded but not added to cache: %s", img_path);
        }
    } 
    else {
        miscLogOutput(__func__, "Failed to load image: %s. SDL_image error: %s", img_path, IMG_GetError());
    }

    return newImage;
}

void cacheClearImageCache() { // free the image cache
    for (int i = 0; i < cache_count; i++) {
        SDL_FreeSurface(img_cache[i].surface);
        img_cache[i].path = NULL;
    }
    miscLogOutput(__func__, "Cache cleared. Total images removed: %d", cache_count);
    cache_count = 0;
}
