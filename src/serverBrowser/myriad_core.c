// system
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#include <stdbool.h>

// onion
#include "utils/str.h"
#include <sqlite3/sqlite3.h>

// local
#include "myriad_core.h"
#include "misc_utils.h"
#include "cache_local.h"

/*
TODO
Add an indexer to fully index cores/paths/crcs that we know the user holds.
Currently i search for the file, names are a big thing across multiple sets of curated romsets
*/

const char *valid_extensions[] = { // probably dont need all these but it's eveyrthing for now
    ".zip", ".rom", ".iso", ".bin", ".gba", ".nes", ".snes", 
    ".7z", ".cue", ".sna", ".dsk", ".kcr", ".atr", ".a26", 
    ".a78", ".j64", ".jag", ".abs", ".cof", ".prg", ".st", 
    ".msa", ".stx", ".dim", ".ipf", ".ri", ".mx1", ".mx2", 
    ".col", ".cas", ".sg", ".sc", ".m3u", ".t64", ".crt", 
    ".nib", ".tap", ".adf", ".hdf", ".lha", ".vec", ".dosz", 
    ".exe", ".com", ".bat", ".ins", ".img", ".ima", ".vhd", 
    ".jrc", ".tc", ".m3u8", ".conf", ".pce", ".sgx", ".ccd", 
    ".d98", ".fdi", ".hdi", ".chd", ".gbc", ".dmg", ".32x", 
    ".sfc", ".smc", ".vb", ".vboy", ".68k", ".mdx", ".md", 
    ".sgd", ".smd", ".gen", ".sms", ".gg", ".mv", ".dx1", 
    ".2d", ".2hd", ".tfd", ".d88", ".88d", ".hdm", ".xdf", 
    ".dup", ".cmd", ".szx", ".z80", ".tzx", ".gz", ".udi", 
    ".mgt", ".trd", ".scl", ".unif", ".unf", ".fds", ".p", 
    ".pbp", ".lnx", ".ws", ".pc2", ".mgw", ".min", ".ngp", 
    ".ngc", ".sv", ".uze", ".sfc", ".vms", ".gb", NULL
};

int is_valid_rom_extension(const char *ext) {
    for (int i = 0; valid_extensions[i] != NULL; i++) {
        if (strcmp(ext, valid_extensions[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

char* searchcorePath(const char* base_path, const char* core_name) { // accepts the base core path and standard core name and finds the path (e.g input of "Nestopia" will return the full path for the core based on the .info)
    static char found_path[STR_MAX];
    memset(found_path, 0, sizeof(found_path));

    struct dirent *entry;
    DIR *dir = opendir(base_path);

    if (dir == NULL) {
        log_output("Failed to open directory");
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        char full_path[STR_MAX + 1];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        struct stat stbuf;
        stat(full_path, &stbuf);

        if (S_ISDIR(stbuf.st_mode) && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char *result = searchcorePath(full_path, core_name);
            if(result) {
                closedir(dir);
                return result;
            }
        } else {
            if (strcmp(entry->d_name, "00_example_libretro.info") == 0) { // ignore this weirdo
                continue;
            }

            if (strstr(entry->d_name, ".info")) {
                FILE *file = fopen(full_path, "r");
                if (file) {
                    char line[256];
                    for (int i = 0; i < 5; i++) {
                        if(!fgets(line, sizeof(line), file)) {
                            break;
                        }
                    }

                    char *start_quote = strchr(line, '"');
                    char *end_quote = start_quote ? strchr(start_quote + 1, '"') : NULL;

                    if (start_quote && end_quote && (end_quote - start_quote - 1 == strlen(core_name)) && strncmp(start_quote+1, core_name, end_quote-start_quote-1) == 0) {
                        char *dot = strrchr(full_path, '.');
                        if (dot && strcmp(dot, ".info") == 0) {
                            *dot = '\0'; 
                            strcat(full_path, ".so");
                        }
                        strncpy(found_path, full_path, sizeof(found_path)-1);
                        fclose(file);
                        closedir(dir);
                        log_output("Found Core: %s", found_path);
                        return found_path;
                    }
                    fclose(file);
                }
            }
        }
    }

    closedir(dir);
    return NULL;
}

char* searchromRecursive(const char* base_path_rom, const char* rom_pattern) { // accepts the base rom path and rom name and returns the rom full path, ***this is a fallback for an SQ search***
    static char found_rom_path[STR_MAX + 1];
    memset(found_rom_path, 0, sizeof(found_rom_path));

    struct dirent *entry;
    DIR *dir = opendir(base_path_rom);

    if (dir == NULL) {
        log_output("Failed to open directory");
        return NULL;
    }

    char modified_pattern[STR_MAX + 1];
    snprintf(modified_pattern, sizeof(modified_pattern), "%s.?*", rom_pattern);

    while ((entry = readdir(dir)) != NULL) {
        char full_path[STR_MAX + 1];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path_rom, entry->d_name);

        struct stat stbuf;
        stat(full_path, &stbuf);

        if (S_ISDIR(stbuf.st_mode) && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char *result = searchromRecursive(full_path, rom_pattern);
            if(result) {
                closedir(dir);
                return result;
            }
        } else {
            if (fnmatch(modified_pattern, entry->d_name, 0) == 0) {
                char *ext = strrchr(entry->d_name, '.');
                if (ext && is_valid_rom_extension(ext)) {
                    if (S_ISREG(stbuf.st_mode)) {
                        log_output("Found valid ROM file at path: %s", full_path);
                        strncpy(found_rom_path, full_path, sizeof(found_rom_path) - 1);
                        found_rom_path[sizeof(found_rom_path) - 1] = '\0';
                        closedir(dir);
                        log_output("Writing %s to found_rom_path in recurseRom", found_rom_path);
                        return found_rom_path;
                    } else {
                        log_output("The found path does not point to a valid file: %s", full_path);
                        return NULL;
                    }
                }
            }
        }
    }

    closedir(dir);
    return NULL;
}

char* searchromPathSQ(const char* base_path_rom, const char* rom_name) {
    static char found_rom_path[STR_MAX];
    memset(found_rom_path, 0, sizeof(found_rom_path));
    
    log_output("romSQ Start: Searching for ROM named %s in base path %s", rom_name, base_path_rom);

    DIR *dir = opendir(base_path_rom);
    if (!dir) {
        log_output("Failed to open directory: %s", base_path_rom);
        return NULL;
    }

    struct dirent *entry;
    char folder_name[32] = {0};
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;

    while ((entry = readdir(dir)) != NULL) {
        memset(folder_name, 0, sizeof(folder_name));
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                strcpy(folder_name, entry->d_name);
                // log_output("Processing folder: %s", folder_name); // too much output

                char db_name[1024];
                snprintf(db_name, sizeof(db_name), "%s/%s/%s_cache6.db", base_path_rom, folder_name, folder_name);

                struct stat db_stat;
                if (stat(db_name, &db_stat) != 0 || db_stat.st_size == 0) {
                    // log_output("Database file %s either doesn't exist or is empty. Skipping...", db_name); // too much output
                    continue;
                }

                rc = sqlite3_open(db_name, &db);
                if (rc) {
                    log_output("Can't open database: %s. Error: %s", db_name, sqlite3_errmsg(db));
                    sqlite3_close(db);
                    continue;
                }

                char table_name[64];
                snprintf(table_name, sizeof(table_name), "%s_roms", folder_name);
                char query[STR_MAX];
                snprintf(query, sizeof(query), "SELECT PATH FROM '%s' WHERE DISP = ?;", table_name);

                rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
                if (rc != SQLITE_OK) {
                    sqlite3_close(db);
                    continue;
                }

                sqlite3_bind_text(stmt, 1, rom_name, -1, SQLITE_STATIC);

                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    strncpy(found_rom_path, (const char *)sqlite3_column_text(stmt, 0), sizeof(found_rom_path) - 1);
                    
                    struct stat file_stat;
                    if (stat(found_rom_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
                        log_output("Found valid ROM file at path: %s", found_rom_path);
                        found_rom_path[sizeof(found_rom_path) - 1] = '\0';

                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                        closedir(dir);
                        log_output("Writing %s to found_rom_path in SQRom", found_rom_path);
                        return found_rom_path;
                    } else {
                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                        continue;
                    }
                } else {
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    continue;
                }
            }
        }
    }

    closedir(dir);
    log_output("ROM not found in any directory.");
    return NULL;
}

char* buildImgPath(const char* rom_path) { // accepts the path of a rom and finds the img file, will move back 1 directory level incase roms are nested (like in some TBS)
    if (!rom_path) {
        log_output("ROM path provided is NULL");
        return NULL;
    }

    static char img_path[1024];
    char base_path[STR_MAX];
    char mutable_base_path[STR_MAX];
    char rom_name[STR_MAX];

    const char *last_slash = strrchr(rom_path, '/'); // there's an issue with struct data at the minute, you'll see "hhh" in some returns of this function
    if (!last_slash) {
        log_output("Invalid ROM path provided: %s", rom_path);
        return NULL;
    }

    size_t base_path_len = last_slash - rom_path;
    if (base_path_len >= sizeof(base_path)) {
        log_output("ROM path too long: %s", rom_path);
        return NULL;
    }

    strncpy(base_path, rom_path, base_path_len);
    base_path[base_path_len] = '\0';
    strncpy(rom_name, last_slash + 1, sizeof(rom_name) - 1);
    rom_name[sizeof(rom_name) - 1] = '\0';

    char* ext_pos = strrchr(rom_name, '.');
    if (ext_pos) {
        *ext_pos = '\0';
    }

    strncpy(mutable_base_path, base_path, STR_MAX - 1);
    mutable_base_path[STR_MAX - 1] = '\0';

    const char* valid_extensions[] = {".png", ".jpg", ".jpeg", NULL};
    bool imageFound = false;

    for (int tries = 0; tries < 3; tries++) {
        char imgs_dir[STR_MAX + 10];
        snprintf(imgs_dir, sizeof(imgs_dir), "%s/Imgs", mutable_base_path);

        for (int i = 0; valid_extensions[i] != NULL; i++) {
            snprintf(img_path, sizeof(img_path), "%s/%s%s", imgs_dir, rom_name, valid_extensions[i]);
            
            struct stat stbuf;
            if (stat(img_path, &stbuf) == 0 && !S_ISDIR(stbuf.st_mode)) {
                log_output("Found img at: %s", img_path);
                imageFound = true;
                return img_path;
            }
        }

        last_slash = strrchr(mutable_base_path, '/');
        if (!last_slash) break;
        mutable_base_path[last_slash - mutable_base_path] = '\0';
    }

    if (!imageFound) {
        log_output("No image found for ROM: %s", rom_path);
    }

    return NULL;
}


// Index all our cores, there's about 101 but i've set the max to 200.... just incase
int coreCount = 0;
coreInfo coreArray[MAX_CORES];

void coreVersionIndexer() {
    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(BASE_PATH_CORE)) == NULL) {
        perror("Failed to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char *ext = strrchr(entry->d_name, '.');
        if (ext && !strcmp(ext, ".info")) {
            char filePath[512];
            sprintf(filePath, "%s/%s", BASE_PATH_CORE, entry->d_name);
            FILE *file = fopen(filePath, "r");
            if (file) {
                char line[STR_MAX];
                int lineCount = 0;
                while (fgets(line, sizeof(line), file) && lineCount <= 9) {
                    lineCount++;
                    if (lineCount == 5) {
                        sscanf(line, "corename = \"%[^\"]\"", coreArray[coreCount].coreName);
                    } else if (lineCount == 9) {
                        sscanf(line, "display_version = \"%[^\"]\"", coreArray[coreCount].version);
                        coreCount++;
                    }
                }
                fclose(file);
            }
        }
    }

    closedir(dir);
}

// give me a core name, i'll give you a core version
const char* coreVersion(const char* coreName) {
    for (int i = 0; i < coreCount; i++) {
        if (strcmp(coreArray[i].coreName, coreName) == 0) {
            return coreArray[i].version;
        }
    }
    return NULL;
}