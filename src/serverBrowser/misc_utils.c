//system
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <stdarg.h> 
#include <stdlib.h>
#include <sys/time.h>

#include <sys/stat.h>

//onion
#include <zlib.h>

//local
#include "misc_utils.h"
#include "parse_json.h"
#include "myriad_core.h"
#include "vault.h"

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

bool miscHasRelay(const char* mitmIP) { // check if the mitm struct member contains a hostname (it'll be longer than 8 chars if so, shorter if not)
    return strlen(mitmIP) > 8;
}

static char cachedVersion[STR_MAX] = {0};

char* miscGetRAMajorVersion() { // pull our RA version info to compare against the host
    if (cachedVersion[0] != '\0') {
        return cachedVersion;
    }

    FILE *file = fopen(RA_VER, "r");
    if (file == NULL) {
        perror("Failed to open the file");
        return NULL;
    }

    char version[STR_MAX];
    if (fgets(version, STR_MAX, file) == NULL) {
        fclose(file);
        perror("Failed to read the version");
        return NULL;
    }
    fclose(file);

    char *lastDot = strrchr(version, '.');
    if (lastDot != NULL) {
        *lastDot = '\0';
    }

    strncpy(cachedVersion, version, STR_MAX - 1);
    cachedVersion[STR_MAX - 1] = '\0';

    return cachedVersion;
}

bool miscHasFileExt(const char* filename, const char* ext) { // does it have a file extension
    return (bool)(strstr(filename, ext) == filename + strlen(filename) - strlen(ext));
}

int miscIsValidExt(const char *ext) { // is it in our valid extensions list
    for (int i = 0; valid_extensions[i] != NULL; i++) {
        if (strcmp(ext, valid_extensions[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

char* miscGet7zCRC(const char* path) { // 7z supports spitting out a crc32 natively 
    char command[512];
    char crcValue[16] = {0};
    FILE* pipe;

    snprintf(command, sizeof(command), 
             ONION_BIN_DIR "/7z l -slt \"%s\" | "
             "awk -F'= ' '/^CRC/ {print $2}'", 
             path);

    pipe = popen(command, "r");
    if (!pipe) {
        return NULL;
    }

    if (!fgets(crcValue, sizeof(crcValue), pipe)) {
        pclose(pipe);
        return NULL;
    }
    pclose(pipe);

    crcValue[strcspn(crcValue, "\n")] = 0;
    miscLogOutput(__func__, "Calculated local rom CRC: %s \n", crcValue);
    
    return strdup(crcValue);
}

unsigned long miscCalculateCRC32(const char* path) { // calculates the crc32 using xcrc logic w/ zlib libs
    FILE* file = fopen(path, "rb");
    if (!file) {
        miscLogOutput(__func__, "File open error");
        return 0;
    }

    uLong crc = crc32(0L, Z_NULL, 0);
    const size_t bufferSize = 32768;
    unsigned char buffer[bufferSize];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, sizeof(unsigned char), bufferSize, file)) > 0) {
        crc = crc32(crc, buffer, bytesRead);
        miscLogOutput(__func__, "Calculated local rom CRC: %lu \n", crc);
    }

    fclose(file);
    return crc;
}

void miscFreeServerGlobal() {
    if (serversGlobal != NULL) {
        free(serversGlobal);
        serversGlobal = NULL;
        serverCountGlobal = 0;
    }
}

bool miscStringContains(const char* str, const char* substr) { // check if a string contains something
    return strstr(str, substr) != NULL;
}

void miscLogOutput(const char *caller, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[BROWSER] [%s] ", caller);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
}

void miscPrintLocalData(const LocalData *data) { // debug code
    miscLogOutput(__func__, "LocalData:\n");
    miscLogOutput(__func__, "\tromPath: %s\n", data->romPath);
    miscLogOutput(__func__, "\tcorePath: %s\n", data->corePath);
    miscLogOutput(__func__, "\timgPath: %s\n", data->imgPath);
    miscLogOutput(__func__, "\tlocalRomCRC: %s\n", data->localRomCRC);
}

void miscPrintServer(const Server *server) { // debug code
    miscLogOutput(__func__, "Server:\n");
    miscLogOutput(__func__, "name: %s\n", server->name);
    miscLogOutput(__func__, "country: %s\n", server->country);
    miscLogOutput(__func__, "game: %s\n", server->game);
    miscLogOutput(__func__, "gameCRC: %s\n", server->gameCRC);
    miscLogOutput(__func__, "core: %s\n", server->core);
    miscLogOutput(__func__, "coreVersion: %s\n", server->coreVersion);
    miscLogOutput(__func__, "coreCRC: %s\n", server->coreCRC);
    miscLogOutput(__func__, "subsystemName: %s\n", server->subsystemName);
    miscLogOutput(__func__, "retroarchVersion: %s\n", server->retroarchVersion);
    miscLogOutput(__func__, "frontend: %s\n", server->frontend);
    miscLogOutput(__func__, "ip: %s\n", server->ip);
    miscLogOutput(__func__, "port: %d\n", server->port);
    miscLogOutput(__func__, "mitmIP: %s\n", server->mitmIP);
    miscLogOutput(__func__, "mitmPort: %d\n", server->mitmPort);
    miscLogOutput(__func__, "mitmSession: %s\n", server->mitmSession);
    miscLogOutput(__func__, "hostMethod: %d\n", server->hostMethod);
    miscLogOutput(__func__, "hasPassword: %d\n", server->hasPassword);
    miscLogOutput(__func__, "hasSpectatePassword: %d\n", server->hasSpectatePassword);
    miscLogOutput(__func__, "connectable: %d\n", server->connectable);
    miscLogOutput(__func__, "isRetroarch: %d\n", server->isRetroarch);
    miscLogOutput(__func__, "created: %s\n", server->created);
    miscLogOutput(__func__, "updated: %s\n", server->updated);
    
    miscPrintLocalData(&server->local);
}

void miscPrintAllServers() { // debug code
    for(int i = 0; i < serverCountGlobal; i++) {
        miscPrintServer(&serversGlobal[i]);
        miscLogOutput(__func__, "\n");
    }
}

