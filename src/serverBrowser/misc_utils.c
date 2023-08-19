//system
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <stdarg.h> 
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/stat.h>

//onion
#include <zlib.h>

//local
#include "misc_utils.h"
#include "parse_json.h"
#include "myriad_core.h"
#include "lobby_data.h"

#define TIMEOUT_SECONDS 0
#define TIMEOUT_USECONDS 500000  // 0.5s
#define ICMP_ECHO_REQUEST 8

unsigned short checksum(void *buffer, int len) {
    unsigned short *buf = buffer;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }

    if (len == 1) {
        sum += *(unsigned char*)buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;

    return result;
}

double get_latency(const char *server_ip) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        perror("Failed to create raw socket");
        exit(1);
    }

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = TIMEOUT_USECONDS;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Error setting socket timeout");
        close(sock);
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    struct icmp icmp_hdr;
    memset(&icmp_hdr, 0, sizeof(icmp_hdr));
    icmp_hdr.icmp_type = ICMP_ECHO_REQUEST;
    icmp_hdr.icmp_cksum = checksum(&icmp_hdr, sizeof(icmp_hdr));

    struct timeval start, end;
    gettimeofday(&start, NULL);

    if (sendto(sock, &icmp_hdr, sizeof(icmp_hdr), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) <= 0) {
        perror("Failed to send");
        close(sock);
        return -1.0;
    }

    char buffer[1024];
    struct sockaddr_in response_addr;
    socklen_t response_addr_len = sizeof(response_addr);
    if (recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&response_addr, &response_addr_len) > 0) {
        gettimeofday(&end, NULL);
        double time_taken = (end.tv_sec - start.tv_sec) * 1e6;
        time_taken = (time_taken + (end.tv_usec - start.tv_usec)) * 1e-6;
        close(sock);
        return time_taken;
    }

    close(sock);
    return -1.0;
}

static char cachedVersion[STR_MAX] = {0};

char* getRAMajorVersion() { // pull our RA version info to compare against the host
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

bool hasRelay(const char* mitmIP) { // check if the mitm struct member contains a hostname (it'll be longer than 8 chars if so, shorter if not)
    return strlen(mitmIP) > 8;
}

bool wlan0Exists() { // quick check to see if wlan0 is active, if it's been disabled it disappears from the net class
    struct stat st;
    if (stat("/sys/class/net/wlan0", &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    } else {
        return false;
    }
}

bool has_file_extension(const char* filename, const char* ext) {
    return (bool)(strstr(filename, ext) == filename + strlen(filename) - strlen(ext));
}

bool isServerReachable(const char* ip) { // checks servers are reachable
    char command[256];
    log_output("Pinging server %s", ip);
    snprintf(command, sizeof(command), "ping -c 1 -w 1 %s", ip);

    int exitStatus = system(command);

    log_output("Finished pinging %s with status: %d", ip, exitStatus);
    return WEXITSTATUS(exitStatus) == 0;
}

char* get7zCRC(const char* path) { // 7z supports spitting out a crc32 natively 
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
    log_output("Found local Rom CRC: %s", crcValue);
    
    return strdup(crcValue);
}

unsigned long calculateCRC32(const char* path) { // calculates the crc32 using xcrc logic w/ zlib libs
    FILE* file = fopen(path, "rb");
    if (!file) {
        log_output("File open error");
        return 0;
    }

    uLong crc = crc32(0L, Z_NULL, 0);
    const size_t bufferSize = 32768;
    unsigned char buffer[bufferSize];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, sizeof(unsigned char), bufferSize, file)) > 0) {
        crc = crc32(crc, buffer, bytesRead);
        log_output("Found local Rom CRC: %lu", crc);
    }

    fclose(file);
    return crc;
}

void freeServerGlobal() {
    if (serversGlobal != NULL) {
        free(serversGlobal);
        serversGlobal = NULL;
        serverCountGlobal = 0;
    }
}

bool stringContains(const char* str, const char* substr) {
    return strstr(str, substr) != NULL;
}

void log_output(const char *format, ...) { // to log to terminal for uart
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[BROWSER] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
}

void printLocalData(const LocalData *data) { // debug code
    log_output("LocalData:\n");
    log_output("\tromPath: %s\n", data->romPath);
    log_output("\tcorePath: %s\n", data->corePath);
    log_output("\timgPath: %s\n", data->imgPath);
    log_output("\tlocalRomCRC: %s\n", data->localRomCRC);
}

void printServer(const Server *server) { // debug code
    log_output("Server:\n");
    log_output("name: %s\n", server->name);
    log_output("country: %s\n", server->country);
    log_output("game: %s\n", server->game);
    log_output("gameCRC: %s\n", server->gameCRC);
    log_output("core: %s\n", server->core);
    log_output("coreVersion: %s\n", server->coreVersion);
    log_output("coreCRC: %s\n", server->coreCRC);
    log_output("subsystemName: %s\n", server->subsystemName);
    log_output("retroarchVersion: %s\n", server->retroarchVersion);
    log_output("frontend: %s\n", server->frontend);
    log_output("ip: %s\n", server->ip);
    log_output("port: %d\n", server->port);
    log_output("mitmIP: %s\n", server->mitmIP);
    log_output("mitmPort: %d\n", server->mitmPort);
    log_output("mitmSession: %s\n", server->mitmSession);
    log_output("hostMethod: %d\n", server->hostMethod);
    log_output("hasPassword: %d\n", server->hasPassword);
    log_output("hasSpectatePassword: %d\n", server->hasSpectatePassword);
    log_output("connectable: %d\n", server->connectable);
    log_output("isRetroarch: %d\n", server->isRetroarch);
    log_output("created: %s\n", server->created);
    log_output("updated: %s\n", server->updated);
    
    printLocalData(&server->local);
}

void printAllServers() { // debug code
    for(int i = 0; i < serverCountGlobal; i++) {
        printServer(&serversGlobal[i]);
        log_output("\n");
    }
}


