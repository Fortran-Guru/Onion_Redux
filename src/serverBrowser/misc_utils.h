#ifndef MISC_UTILS_H
#define MISC_UTILS_H

#include <stdbool.h>
#include "utils/str.h"
#include "vault.h"

#define ONION_BIN_DIR "/mnt/SDCARD/.tmp_update/bin"
#define RA_VER "/mnt/SDCARD/RetroArch/onion_ra_version.txt"

extern bool quit;

// comms checks 

double get_latency(const char *server_ip);

bool wlan0Exists();

typedef struct {
    bool isReachable; 
    float averageLatency;
} PingResult;

PingResult getServerLatency(const char* ip);
bool isServerReachable(const char* ip);

// crc generation

unsigned long calculateCRC32(const char* path);

char* get7zCRC(const char* source);

bool has_file_extension(const char* filename, const char* ext);

// logging/debug

void log_output(const char *format, ...); // TODO - align with onion logging
void printLocalData(const LocalData *data);
void printServer(const Server *server);
void printAllServers();

// freeing

void freeServerGlobal();

// RA_VER

char* getRAMajorVersion(void);

// RELAY CHECK

bool hasRelay(const char* mitmIP);

// struct check
bool stringContains(const char* str, const char* substr);

#endif // MISC_UTILS_H