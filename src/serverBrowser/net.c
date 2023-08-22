#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include "utils/str.h"

#include "misc_utils.h"

#define TIMEOUT_SECONDS 0
#define TIMEOUT_USECONDS 500000 
#define ICMP_ECHO_REQUEST 8
#define BROADCAST_ADDRESS "255.255.255.255"
#define BROADCAST_PORT 55435
#define DISCOVERY_MAGIC "RANQ"
#define BUFFER_SIZE 5096  // Update buffer size to accommodate longer messages

bool netIsServerReachable(const char* ip) { // checks servers are reachable
    char command[STR_MAX];
    miscLogOutput(__func__, "Pinging server %s", ip);
    snprintf(command, sizeof(command), "ping -c 1 -w 1 %s", ip);

    int exitStatus = system(command);

    miscLogOutput(__func__, "Finished pinging %s with status: %d", ip, exitStatus);
    return WEXITSTATUS(exitStatus) == 0;
}

void netQueryLan() {
    miscLogOutput(__func__, "Starting LAN server search...");
    int sock;
    struct sockaddr_in serverAddr;
    int sendPort;
    char discoveryMagic[] = DISCOVERY_MAGIC;
    char buffer[BUFFER_SIZE];
    char readableBuffer[BUFFER_SIZE];
    ssize_t recvLen;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        miscLogOutput(__func__, "Failed to create socket");
        return;
    }

    srand(time(NULL));
    sendPort = rand() % (65535 - 1024 + 1) + 1024;

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(sendPort);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        miscLogOutput(__func__, "Failed to bind socket");
        close(sock);
        return;
    }
    
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        miscLogOutput(__func__, "Failed to set socket timeout");
        close(sock);
        return;
    }

    int broadcastEnable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        miscLogOutput(__func__, "Failed to enable broadcasting");
        close(sock);
        return;
    }

    struct sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(BROADCAST_PORT);
    if (inet_aton(BROADCAST_ADDRESS, &broadcastAddr.sin_addr) == 0) {
        miscLogOutput(__func__, "Invalid broadcast address");
        close(sock);
        return;
    }

    if (sendto(sock, discoveryMagic, strlen(discoveryMagic), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr)) < 0) {
        miscLogOutput(__func__, "Failed to send discovery packet");
        close(sock);
        return;
    }
    
    do {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        recvLen = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientLen);
        if (recvLen < 0) {
            miscLogOutput(__func__, "Failed to receive data");
            break;
        }

        serversGlobal = realloc(serversGlobal, (serverCountGlobal + 1) * sizeof(Server));
        if (serversGlobal == NULL) {
            miscLogOutput(__func__, "Failed to allocate memory for serversGlobal");
            close(sock);
            return;
        }

        Server *server = &serversGlobal[serverCountGlobal];
        memset(server, 0, sizeof(Server)); 

        for (ssize_t i = 0; i < recvLen; ++i) {
            if (buffer[i] == '\x00') {
                readableBuffer[i] = ' ';
            } else if (isprint((unsigned char)buffer[i])) {
                readableBuffer[i] = buffer[i];
            } else {
                readableBuffer[i] = '.';
            }
        }
        readableBuffer[recvLen] = '\0';

        char *token, *startPtr, *endPtr;
        char *fields[] = {"[request]= ", "[..]= ", "[host]= ", "[platform]= ", "[core]= ", "[corever]= ", "[retroarchv]= ", "[rom]= "};
        int fieldIndex = 0;

        startPtr = readableBuffer;
        strncpy(server->ip, inet_ntoa(clientAddr.sin_addr), 23);
        while (startPtr < readableBuffer + recvLen && fieldIndex < sizeof(fields) / sizeof(fields[0])) {
            endPtr = startPtr;
            while (*endPtr != ' ' || *(endPtr + 1) != ' ') {
                endPtr++;
            }

            char temp = *endPtr;
            *endPtr = '\0';
            token = startPtr;

            switch (fieldIndex) {
                case 2: strncpy(server->name, token, 95); break;
                case 3: strncpy(server->frontend, token, 11); break;
                case 4: strncpy(server->core, token, 95); break;
                case 5: {
                    char *spacePos = strchr(token, ' '); // splitting off the cores version and crc so we can match against it in the struct 
                    if (spacePos) {
                        *spacePos = '\0'; 
                        strncpy(server->coreVersion, token, sizeof(server->coreVersion) - 1);
                        strncpy(server->coreCRC, spacePos + 1, sizeof(server->coreCRC) - 1);
                    }
                    break;
                }
                case 6: strncpy(server->retroarchVersion, token, 11); break;
                case 7: strncpy(server->game, token, 95); break;
            }

            miscLogOutput(__func__, "%s%s", fields[fieldIndex++], token);
            
            server->connectable = 1;
            strncpy(server->country, "LAN", sizeof(server->country) - 1); // just a breadcrumb for us to pick up in main

            *endPtr = temp;
            startPtr = endPtr + 1;
            while (*startPtr == ' ') {
                startPtr++;
            }
        }

        serverCountGlobal++;
    } while (recvLen >= 0);

    close(sock);
    miscLogOutput(__func__, "LAN server search ended");
}

bool netWlan0Exists() { // quick check to see if wlan0 is active, if it's been disabled it disappears from the net class
    struct stat st;
    if (stat("/sys/class/net/wlan0", &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    } else {
        return false;
    }
}

unsigned short checksum(void *buffer, int len) { // checksum for the ping, not really required.. only checking for "any" data back.
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

double netGetLatency(const char *server_ip) { // ping a host, check the time for any sort of reply
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        miscLogOutput(__func__, "Failed to create raw socket");
        exit(1);
    }

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = TIMEOUT_USECONDS;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        miscLogOutput(__func__, "Error setting socket timeout");
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
        miscLogOutput(__func__, "Failed to send");
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

