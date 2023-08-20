#ifndef MISC_UTILS_H
#define MISC_UTILS_H

#include <stdbool.h>
#include "utils/str.h"
#include "vault.h"

#include "cjson/cJSON.h"

#define ONION_BIN_DIR "/mnt/SDCARD/.tmp_update/bin"
#define RA_VER "/mnt/SDCARD/RetroArch/onion_ra_version.txt"

extern bool quit;

// comms checks 

double miscGetLatency(const char *server_ip);

bool miscWlan0Exists();

typedef struct {
    bool isReachable; 
    float averageLatency;
} PingResult;

PingResult getServerLatency(const char* ip);
bool miscIsServerReachable(const char* ip);

// crc generation

unsigned long miscCalculateCRC32(const char* path);
char* miscGet7zCRC(const char* source);
bool miscHasFileExt(const char* filename, const char* ext);
int miscIsValidExt(const char *ext);

// logging/debug

void miscLogOutput(const char *format, ...); // TODO - align with onion logging
void miscPrintLocalData(const LocalData *data);
void miscPrintServer(const Server *server);
void miscPrintAllServers();

// freeing

void miscFreeServerGlobal();

// RA_VER

char* miscGetRAMajorVersion(void);

// RELAY CHECK

bool miscHasRelay(const char* mitmIP);

// struct check
bool miscStringContains(const char* str, const char* substr);

#endif // MISC_UTILS_H