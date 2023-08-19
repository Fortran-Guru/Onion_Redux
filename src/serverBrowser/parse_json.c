// system
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

// onion
#include "utils/str.h"
#include "cjson/cJSON.h"

// local
#include "parse_json.h"
#include "misc_utils.h"
#include "lobby_data.h"

#define JSON_STRING_LEN 256
#define JSON_FORMAT_NUMBER "    \"%s\": %d,\n"
#define JSON_FORMAT_NUMBER_NC "    \"%s\": %d\n"
#define JSON_FORMAT_STRING "    \"%s\": \"%s\",\n"
#define JSON_FORMAT_STRING_NC "    \"%s\": \"%s\"\n"
#define JSON_FORMAT_TAB_NUMBER "	\"%s\":	%d,\n"
#define JSON_FORMAT_TAB_NUMBER_NC "	\"%s\":	%d\n"
#define JSON_FORMAT_TAB_STRING "	\"%s\":	\"%s\",\n"
#define JSON_FORMAT_TAB_STRING_NC "	\"%s\":	\"%s\"\n"

// having to repeat some stuff and change the function name as including headers leads to multiple defs with how the codebase handles them but cjson removes reliance on jansson and yet another lib.
// PHASE These out eventually vvvv 

bool exists_new(const char *file_path)
{
    struct stat buffer;
    return stat(file_path, &buffer) == 0;
}

const char *file_reader(const char *path)
{
    FILE *f = NULL;
    char *buffer = NULL;
    long length = 0;

    if (!exists_new(path))
        return NULL;

    if ((f = fopen(path, "rb"))) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (char *)malloc((length + 1) * sizeof(char));
        if (buffer)
            fread(buffer, sizeof(char), length, f);
        fclose(f);
    }
    buffer[length] = '\0';

    return buffer;
}

bool json_getStringNew(cJSON *object, const char *key, char *dest)
{
    cJSON *json_object = cJSON_GetObjectItem(object, key);
    if (json_object) {
        strncpy(dest, cJSON_GetStringValue(json_object), JSON_STRING_LEN - 1);
        return true;
    }
    return false;
}

bool json_getBoolNew(cJSON *object, const char *key, bool *dest)
{
    cJSON *json_object = cJSON_GetObjectItem(object, key);
    if (json_object) {
        *dest = cJSON_IsTrue(json_object);
        return true;
    }
    return false;
}

bool json_getIntNew(cJSON *object, const char *key, int *dest)
{
    cJSON *json_object = cJSON_GetObjectItem(object, key);
    if (json_object) {
        *dest = (int)cJSON_GetNumberValue(json_object);
        return true;
    }
    return false;
}

cJSON *json_loadNew(const char *file_path)
{
    return cJSON_Parse(file_reader(file_path));
}

void json_saveNew(cJSON *object, char *file_path)
{
    if (object == NULL || file_path == NULL)
        return;

    char *output = cJSON_Print(object);

    FILE *fp = NULL;
    if ((fp = fopen(file_path, "w+")) != NULL) {
        fwrite(output, strlen(output), 1, fp);
        fclose(fp);
    }

    if (output != NULL)
        cJSON_free(output);
}

// PHASE These out eventually ^^^^ 

bool retryRetrieve = true;
Server* serversGlobal = NULL;
int serverCountGlobal = 0;

void error(const char* msg);

char* sendHttpRequest(const char* hostname, int portno, const char* message_fmt) { // standard function for pulling json data from a website, opens socket, sends request, returns goodies
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent* server;

    char buffer[65536];
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd < 0) {
        log_output("ERROR opening socket");
        return NULL;
    }

    log_output("Socket created with fd: %d", sockfd);

    server = gethostbyname(hostname);
    
    if (server == NULL) {
        log_output("ERROR, no such host");
        close(sockfd);
        return NULL;
    }

    log_output("Host %s found", hostname);
    
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char*)server->h_addr,
          (char*)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);
    
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        log_output("ERROR connecting to %s:%d", hostname, portno);
        close(sockfd);
        return NULL;
    }

    log_output("Connected to %s:%d", hostname, portno);

    sprintf(buffer, message_fmt, hostname);
    n = write(sockfd, buffer, strlen(buffer));
    
    if (n < 0) {
        log_output("ERROR writing to socket");
        close(sockfd);
        return NULL;
    }

    bzero(buffer, 65536);

    char* response = NULL;
    size_t response_length = 0;

    while ((n = read(sockfd, buffer, 4095)) > 0) {
        buffer[n] = '\0';
        response = realloc(response, response_length + n + 1);
        if (response == NULL) {
            log_output("Memory allocation failed");
            close(sockfd);
            return NULL;
        }
        strcpy(response + response_length, buffer);
        response_length += n;
        log_output("Received %d bytes from server", n);
    }

    if (n < 0) {
        log_output("ERROR reading from socket");
        free(response);
        close(sockfd);
        return NULL;
    }

    log_output("Total response length: %zu bytes", response_length);

    close(sockfd);
    log_output("Connection closed");
    return response;
}

void parse_json(const char *json_text) {
    cJSON *root;
    cJSON *data, *fields;
    int array_size, i;

    root = cJSON_Parse(json_text);

    if (!root) {
        log_output("error: failed to parse JSON");
        return;
    }

    array_size = cJSON_GetArraySize(root);

    for (i = 0; i < array_size; i++) {
        data = cJSON_GetArrayItem(root, i);
        fields = cJSON_GetObjectItem(data, "fields");

        char username[JSON_STRING_LEN], country[JSON_STRING_LEN], game_name[JSON_STRING_LEN];
        char game_crc[JSON_STRING_LEN], core_name[JSON_STRING_LEN], core_version[JSON_STRING_LEN];
        char subsystem_name[JSON_STRING_LEN], retroarch_version[JSON_STRING_LEN], frontend[JSON_STRING_LEN];
        char ip[JSON_STRING_LEN], mitm_ip[JSON_STRING_LEN], mitm_session[JSON_STRING_LEN];
        char created[JSON_STRING_LEN], updated[JSON_STRING_LEN];
        int port, mitm_port, host_method;
        bool has_password, has_spectate_password, connectable, is_retroarch;

        json_getStringNew(fields, "username", username);
        json_getStringNew(fields, "country", country);
        json_getStringNew(fields, "game_name", game_name);
        json_getStringNew(fields, "game_crc", game_crc);
        json_getStringNew(fields, "core_name", core_name);
        json_getStringNew(fields, "core_version", core_version);
        json_getStringNew(fields, "subsystem_name", subsystem_name);
        json_getStringNew(fields, "retroarch_version", retroarch_version);
        json_getStringNew(fields, "frontend", frontend);
        json_getStringNew(fields, "ip", ip);
        json_getIntNew(fields, "port", &port);
        json_getStringNew(fields, "mitm_ip", mitm_ip);
        json_getIntNew(fields, "mitm_port", &mitm_port);
        json_getStringNew(fields, "mitm_session", mitm_session);
        json_getIntNew(fields, "host_method", &host_method);
        json_getBoolNew(fields, "has_password", &has_password);
        json_getBoolNew(fields, "has_spectate_password", &has_spectate_password);
        json_getBoolNew(fields, "connectable", &connectable);
        json_getBoolNew(fields, "is_retroarch", &is_retroarch);
        json_getStringNew(fields, "created", created);
        json_getStringNew(fields, "updated", updated);

        // build the struct
		serversGlobal = realloc(serversGlobal, (serverCountGlobal + 1) * sizeof(Server));
        if (!serversGlobal) {
            log_output("Failed to allocate memory");
            return;
        }

        Server* newServer = &serversGlobal[serverCountGlobal];
        strcpy(newServer->name, username);
        strcpy(newServer->country, country);
        strcpy(newServer->game, game_name);
        strcpy(newServer->gameCRC, game_crc);
        strcpy(newServer->core, core_name);
        
        strcpy(newServer->coreVersion, core_version);
        
        char core_version_copy[100]; // the core version and name are both on one line on the returned info, split em into seperate struct members
        strcpy(core_version_copy, core_version);
        char *core_version_token = strtok(core_version_copy, " ");
        if(core_version_token) {
            strcpy(newServer->coreVersion, core_version_token);
            core_version_token = strtok(NULL, " ");
            if(core_version_token)
                strcpy(newServer->coreCRC, core_version_token);
        }

        strcpy(newServer->subsystemName, subsystem_name);
        strcpy(newServer->retroarchVersion, retroarch_version);
        strcpy(newServer->frontend, frontend);
        strcpy(newServer->ip, ip);
        newServer->port = port;
        strcpy(newServer->mitmIP, mitm_ip);
        newServer->mitmPort = mitm_port;
        strcpy(newServer->mitmSession, mitm_session);
        newServer->hostMethod = host_method;
        newServer->hasPassword = has_password;
        newServer->hasSpectatePassword = has_spectate_password;
        newServer->connectable = connectable;
        newServer->isRetroarch = is_retroarch;
        strcpy(newServer->created, created);
        strcpy(newServer->updated, updated);
		
		// old struct testing logic at point of json
		
		// log_output("Name: %s\n", newServer->name);
        // log_output("Country: %s\n", newServer->country);
        // log_output("Game: %s\n", newServer->game);
        // log_output("Game CRC: %s\n", newServer->gameCRC);
        // log_output("Core: %s\n", newServer->core);
        // log_output("Core Version: %s\n", newServer->coreVersion);
        // log_output("Core CRC: %s\n", newServer->coreCRC);
        // log_output("Subsystem Name: %s\n", newServer->subsystemName);
        // log_output("Retroarch Version: %s\n", newServer->retroarchVersion);
        // log_output("Frontend: %s\n", newServer->frontend);
        // log_output("IP: %s\n", newServer->ip);
        // log_output("Port: %d\n", newServer->port);
        // log_output("MITM IP: %s\n", newServer->mitmIP);
        // log_output("MITM Port: %d\n", newServer->mitmPort);
        // log_output("MITM Session: %s\n", newServer->mitmSession);
        // log_output("Host Method: %d\n", newServer->hostMethod);
        // log_output("Has Password: %s\n", newServer->hasPassword ? "true" : "false");
        // log_output("Has Spectate Password: %s\n", newServer->hasSpectatePassword ? "true" : "false");
        // log_output("Connectable: %s\n", newServer->connectable ? "true" : "false");
        // log_output("Is Retroarch: %s\n", newServer->isRetroarch ? "true" : "false");
        // log_output("Created: %s\n", newServer->created);
        // log_output("Updated: %s\n", newServer->updated);

        // log_output("\n\n");

        serverCountGlobal++;
    }

    cJSON_Delete(root);
}

void retrieveData() { // will retry ONCE if it fails.
    if (!retryRetrieve) {
        log_output("Something went wrong with the json retrieval... retrying one more time.");
        return;
    }

    char* hostname = "lobby.libretro.com";
    int portno = 80;
    char* message_fmt = "GET /list HTTP/1.1\r\nHost: %s\r\nUser-Agent: MiyooMiniPlusOnion4.3\r\nConnection: close\r\n\r\n";

    char* response = sendHttpRequest(hostname, portno, message_fmt);
    if (response == NULL) {
        log_output("ERROR: Failed to retrieve server response\n");
        return;
    }

    char* json_start = strstr(response, "[");
    if (json_start == NULL) {
        log_output("ERROR: JSON content not found\n");
        free(response);
        retryRetrieve = false;
        retrieveData();
        return;
    }

    char* json_end = strrchr(response, ']');
    if (json_end == NULL) {
        log_output("ERROR: End of JSON not found\n");
        free(response);
        retryRetrieve = false;
        retrieveData();
        return;
    }

    *(json_end + 1) = '\0';
    
    log_output("Parsing data");
    parse_json(json_start);
    log_output("Found %d servers", serverCountGlobal);
    log_output("Freeing response data, if it exists.");
    free(response);
    dataRetrieved = true;
}
