#ifndef parseLobbyData_H
#define parseLobbyData_H

#include "cjson/cJSON.h"

extern bool dataRetrieved;
char* parseSendHTTPReq(const char* hostname, int portno, const char* message_fmt);
void parseRetrieveData();
bool json_getString(cJSON *object, const char *key, char *dest);
bool parseGetBool(cJSON *object, const char *key, bool *dest);
bool parseGetInt(cJSON *object, const char *key, int *dest);

void parseLobbyData(const char* json_text);

#endif
