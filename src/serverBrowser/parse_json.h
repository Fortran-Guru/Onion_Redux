#ifndef PARSE_JSON_H
#define PARSE_JSON_H

#include "cjson/cJSON.h"

extern bool dataRetrieved;
char* sendHttpRequest(const char* hostname, int portno, const char* message_fmt);
void retrieveData();
bool json_getStringNew(cJSON *object, const char *key, char *dest);
bool json_getBoolNew(cJSON *object, const char *key, bool *dest);
bool json_getIntNew(cJSON *object, const char *key, int *dest);

void parse_json(const char* json_text);

#endif
