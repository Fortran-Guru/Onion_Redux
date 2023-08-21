#ifndef NET_H
#define NET_H

#include <stdbool.h>

bool netIsServerReachable(const char* ip);
void netQueryLan();
void* netQueryLanThread(void* arg);
bool netHasRelay(const char* mitmIP);
bool netWlan0Exists();
unsigned short checksum(void *buffer, int len);
double netGetLatency(const char *server_ip);

#endif // NET_H
