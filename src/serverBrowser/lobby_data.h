#ifndef LOBBY_DATA_H
#define LOBBY_DATA_H

// Below holds the cores and versions

typedef struct {
    char coreName[50];
    char version[50];
} coreInfo;

// Below holds local data but is part of the server struct

typedef struct {
    char romPath[STR_MAX];
    char corePath[STR_MAX];
    char imgPath[STR_MAX];
    char localRomCRC[12];
    char coreName[50];
    char version[50];
} LocalData;

// Below holds data we parse from the json pull from lobby

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