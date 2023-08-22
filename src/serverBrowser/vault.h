#ifndef VAULT_H
#define VAULT_H

// Below holds the cores and version we pull with myriadCoreVersionIndexer
typedef struct {
    char coreName[50];
    char version[50];
} coreInfo;

// Local data 
typedef struct {
    char romPath[STR_MAX];
    char corePath[STR_MAX];
    char imgPath[STR_MAX];
    char localRomCRC[12];
    char coreName[50];
    char version[50];
} LocalData;

// Global servers
typedef struct {
    char name[96];
    char country[8];
    char game[96];
    char gameCRC[16];
    char core[96];
    char coreVersion[12];
    char coreCRC[24];
    char subsystemName[24];
    char retroarchVersion[12];
    char frontend[12];
    char ip[24];
    int port;
    char mitmIP[64];
    int mitmPort;
    char mitmSession[100];
    int hostMethod;
    int hasPassword;
    int hasSpectatePassword;
    int connectable;
    int isRetroarch;
    char created[32];
    char updated[32];
    LocalData local;
} Server;

extern Server* serversGlobal;
extern int serverCountGlobal;

#endif