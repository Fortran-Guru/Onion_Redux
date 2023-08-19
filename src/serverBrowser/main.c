// system
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <linux/input.h>
#include <stdbool.h>

// onion
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include "components/list.h"
#include "system/battery.h"
#include "system/display.h"
#include "system/keymap_hw.h"
#include "system/keymap_sw.h"
#include "system/lang.h"
#include "system/settings.h"
#include "theme/sound.h"
#include "theme/theme.h"
#include "utils/keystate.h"
#include "utils/log.h"
#include "utils/sdl_init.h"
#include "utils/str.h"

//local
#include "myriad_core.h"
#include "misc_utils.h"
#include "parse_json.h"
#include "gui_draw.h"
#include "lobby_data.h"
#include "cache_local.h"

#define FRAMES_PER_SECOND 60
#define SHUTDOWN_TIMEOUT 500

/*
TODO
Build a struct that contains all of our core versions
*/


bool startRetroarch = true;
bool dataRetrieved = false;
bool dialog_open = false;
bool lobbyReachable = true; 
bool wifiDisabled = false;
bool quit = false;
bool firstDialog = false;
static KeyState keystate[320] = {(KeyState)0};

static void sigHandler(int sig)
{
    switch (sig) {
    case SIGINT:
        quit = true;
        break;
    case SIGTERM:
        quit = true;
        break;
    default:
        break;
    }
}

void showDialog(const char *title, const char *message) {
    if (!firstDialog) {
        firstDialog = true;
        dialog_open = true;
        startRetroarch = false;
        bool show_hint = false;
        theme_renderDialog(screen, title, message, show_hint);
    }
}

void actionJoinServer(void *item) {
    static char cmd_line[1024] = {0};
    ListItem *listItem = (ListItem *)item;
    Server *server = (Server *)listItem->payload_ptr;
    
    // debug
    
    log_output("Attempting to join server: %s at IP %s with port %d", server->name, server->ip, server->port);
    log_output("Hosts rom is: %s with CRC of %s, your rom is: %s", server->game, server->gameCRC, server->local.localRomCRC);
    log_output("Hosts core is: %s with CRC/Commit of %s", server->core, server->coreCRC);
    log_output("Hosts RetroArch version is %s", server->retroarchVersion);
    log_output("Hosts frontend (OS) is %s", server->frontend);
    log_output("Hosts server has relay? %s with hostname of %s and port of %d", server->mitmSession, server->mitmIP, server->mitmPort); 
    log_output("Hosts server has password/is private? %d", server->hasPassword);
    log_output("Hosts server is connectable? %d", server->connectable);
    log_output("Local rom path is: %s", server->local.romPath);
    
    if (server->connectable == 0) {
        showDialog("Connection Error", "This server is not connectable. \n \n Please try another server.");
    }

    if (strlen(server->local.romPath) == 0) {
        showDialog("ROM Not Found", "Rom not found. Cannot proceed. \n");
    }

    if (strlen(server->local.corePath) == 0) {
        showDialog("Core Not Found", "Core not found. \n \n Cannot proceed.");
    }

    if (strcmp(server->local.localRomCRC, server->gameCRC) != 0) {
        char message[STR_MAX];
        snprintf(message, sizeof(message), "ROM CRC mismatch. \n \n Expected: %s, \n Found: %s. \n \n Cannot proceed.", server->gameCRC, server->local.localRomCRC);
        showDialog("ROM Mismatch", message);
    }

    if (hasRelay(server->mitmIP)) {
        showDialog("Connection Error", "Relay servers not currently supported. \n \n Please try another server.");
    }

    if (startRetroarch) {      
        char serverIP[50];
        
        if (hasRelay(server->mitmIP)) {
            strcpy(serverIP, server->mitmIP);
        } else {
            strcpy(serverIP, server->ip);
        }
        
        snprintf(cmd_line, sizeof(cmd_line), 
                 "HOME=/mnt/SDCARD/RetroArch cd /mnt/SDCARD/RetroArch && ./retroarch -C %s -v -L \"%s\" \"%s\"",
                 serverIP, server->local.corePath, server->local.romPath);

        log_output("Prepared command: %s", cmd_line);

        log_output("CMD_OUTPUT: %s\n", cmd_line);
        freeServerGlobal();
        quit = true;
    }
    firstDialog = false;
}


int main(int argc, char *argv[])
{
    coreVersionIndexer();
    
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    char title_str[STR_MAX] = "Onion Server Browser";
    char *majorVersion = getRAMajorVersion();
    
    int selected = 0;
    int i;
    bool required = false;
    
    SDL_InitDefault(true);
    settings_load();
    lang_load();
    serverCountGlobal = 0;
       
    if (!majorVersion) {
        log_output("Error retrieving the major version.\n");
        return 1;
    }
    
    theme_renderFooter(screen);
    theme_renderHeaderBattery(screen, battery_getPercentage());
    theme_renderDialog(screen, "Searching...", "Searching for servers...", true); // build a landing page before we cache, find, build the list.

    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
            
    if (wlan0Exists()) {
        bool serverIsReachable = isServerReachable("34.102.164.250"); // RA lobby server IP address 34.102.164.250, test no connection with 123.231.123.231

        if(serverIsReachable) {
            log_output("Reachable! Sending data request");
            retrieveData();
            log_output("Building server list");
        } else {
            log_output("Unable to reach server, cannot continue");
            lobbyReachable = false;
            strncpy(title_str, "No Connection", STR_MAX);
        }
    } else {
        log_output("Wifi disabled? wlan0 not found");
        strncpy(title_str, "Wifi Disabled", STR_MAX);
        wifiDisabled = true;
    }
    
    int battery_percentage = battery_getPercentage();
    List list = list_create(serverCountGlobal, LIST_TINY);
    
    printAllServers(); //debug
      
    if (serverCountGlobal > 0 && serversGlobal != NULL) {
        for (i = 0; i < serverCountGlobal; i++) {
            serversGlobal[i].local.romPath[0] = '\0';
            serversGlobal[i].local.corePath[0] = '\0';
            serversGlobal[i].local.imgPath[0] = '\0';
            serversGlobal[i].local.localRomCRC[0] = '\0';
            
            // checks ping, but most servers don't actually reply - will integrate this later
            
            // double latency = get_latency(serversGlobal[i].ip);
            // if (latency >= 0) {
                // log_output("Estimated latency to %s: %.6f seconds", serversGlobal[i].ip, latency);
            // } else {
                // log_output("Failed to estimate latency to %s.", serversGlobal[i].ip);
            // }
            
            char prefix[50] = "";
            if(serversGlobal[i].connectable == 0) {
                strcpy(prefix, "[Not connectable] ");
            } else {
                if(serversGlobal[i].hasPassword != 0) {
                    strcpy(prefix, "[Passworded] ");
                }
                
                char* found_rom_path = lookupRomCacheLocal(serversGlobal[i].game);
                if (!found_rom_path) {
                    found_rom_path = searchromPathSQ(BASE_PATH_ROM, serversGlobal[i].game);
                    if (!found_rom_path) {
                        found_rom_path = searchromRecursive(BASE_PATH_ROM, serversGlobal[i].game);
                    }
                    if (found_rom_path) {
                        addRomToCacheLocal(serversGlobal[i].game, found_rom_path);
                    }
                }

                if (found_rom_path) {
                    strncpy(serversGlobal[i].local.romPath, found_rom_path, sizeof(serversGlobal[i].local.romPath) - 1);
                    serversGlobal[i].local.romPath[sizeof(serversGlobal[i].local.romPath) - 1] = '\0';
                }

                char* found_core_path = searchcorePath(BASE_PATH_CORE, serversGlobal[i].core);
                if (found_core_path) {
                    strncpy(serversGlobal[i].local.corePath, found_core_path, sizeof(serversGlobal[i].local.corePath) - 1);
                    serversGlobal[i].local.corePath[sizeof(serversGlobal[i].local.corePath) - 1] = '\0';
                }
                
                if(serversGlobal[i].local.romPath[0] != '\0') {
                    if (has_file_extension(serversGlobal[i].local.romPath, ".zip") || has_file_extension(serversGlobal[i].local.romPath, ".7z")) {
                        char *tempCRC = get7zCRC(serversGlobal[i].local.romPath);
                        if (tempCRC) {
                            strncpy(serversGlobal[i].local.localRomCRC, tempCRC, sizeof(serversGlobal[i].local.localRomCRC) - 1);
                            serversGlobal[i].local.localRomCRC[sizeof(serversGlobal[i].local.localRomCRC) - 1] = '\0';
                            free(tempCRC);
                        }
                    } else {
                        unsigned long rom_crc = calculateCRC32(serversGlobal[i].local.romPath);
                        snprintf(serversGlobal[i].local.localRomCRC, sizeof(serversGlobal[i].local.localRomCRC), "%08lX", rom_crc);
                    }

                    char* imgFilePath = buildImgPath(serversGlobal[i].local.romPath);
                    if (imgFilePath) {
                        strncpy(serversGlobal[i].local.imgPath, imgFilePath, sizeof(serversGlobal[i].local.imgPath) - 1);
                        serversGlobal[i].local.imgPath[sizeof(serversGlobal[i].local.imgPath) - 1] = '\0';
                    }
                } 
                else {
                    log_output("No romPath available for server: %s", serversGlobal[i].name);
                }
            }

            char fullLabel[STR_MAX];
            snprintf(fullLabel, sizeof(fullLabel), "%s%s - %s", prefix, serversGlobal[i].name, serversGlobal[i].game);

            ListItem item = {
                .action_id = i,
                .action = actionJoinServer,
                .payload_ptr = &serversGlobal[i],
                .disabled = serversGlobal[i].connectable == 0
            };
            snprintf(item.label, STR_MAX + 1, "%s", fullLabel);
            list_addItem(&list, item);
        }
        
    }
    
    // printAllServers(); //debug
   

    list_scrollTo(&list, selected);

    bool has_title = strlen(title_str) > 0;

    bool list_changed = true;
    bool header_changed = true;
    bool footer_changed = true;
    bool battery_changed = true;
    bool refresh_head_foot = true;

    SDLKey changed_key;
    bool key_changed = false;

    uint32_t acc_ticks = 0, last_ticks = SDL_GetTicks(),
             time_step = 1000 / FRAMES_PER_SECOND;

    while (!quit) {
        uint32_t ticks = SDL_GetTicks();
        acc_ticks += ticks - last_ticks;
        last_ticks = ticks;

        if (dataRetrieved) {
            list_changed = true;
            refresh_head_foot = true;
            dataRetrieved = false;
        }

        if (updateKeystate(keystate, &quit, true, &changed_key)) {
            if (keystate[SW_BTN_DOWN] >= PRESSED) {
                if (dialog_open) {
                    list_changed = true;
                    refresh_head_foot = true;
                    dialog_open = false;
                } else {
                    key_changed =
                        list_keyDown(&list, keystate[SW_BTN_DOWN] == REPEATING);
                    list_changed = true;

                }
            }
            else if (keystate[SW_BTN_UP] >= PRESSED) {
                if (dialog_open) {
                    list_changed = true;
                    refresh_head_foot = true;
                    dialog_open = false;
                } else {
                    key_changed =
                        list_keyUp(&list, keystate[SW_BTN_UP] == REPEATING);
                    list_changed = true;
                }
            }
            
            if (changed_key == SW_BTN_A && keystate[SW_BTN_A] == RELEASED) {
                if (dialog_open) {
                    refresh_head_foot = true;
                    list_changed = true;
                    dialog_open = false;
                } else {
                    ListItem *selectedItem = list_currentItem(&list);
                    if (selectedItem && selectedItem->action) {
                        selectedItem->action(selectedItem);
                    }
                }
            }

            if (!required && changed_key == SW_BTN_B && keystate[SW_BTN_B] == RELEASED) {
                if (dialog_open) {
                    list_changed = true;
                    refresh_head_foot = true;
                    dialog_open = false;
                } else {
                    freeServerGlobal();
                    quit = true;
                }
            }
        }

        if (key_changed || quit) {
            sound_change();
            key_changed = false;
        }

        if (quit)
            break;
        
        if (refresh_head_foot) {
            footer_changed = true;
            header_changed = true;
        }

        if (battery_hasChanged(ticks, &battery_percentage))
            battery_changed = true;

        if (acc_ticks >= time_step) {
            if (header_changed || battery_changed) {
                theme_renderHeader(screen, has_title ? title_str : NULL,
                                   !has_title);
            }
            
            if (list_changed) {
                theme_renderList(screen, &list);
                ListItem *selectedItem = list_currentItem(&list);
                if (selectedItem) {
                    Server *selectedServer = (Server *)selectedItem->payload_ptr;
                    
                    char truncatedFrontend[14];

                    if (strlen(selectedServer->frontend) > 10) {
                        strncpy(truncatedFrontend, selectedServer->frontend, 10);
                        truncatedFrontend[10] = '\0';
                        strcat(truncatedFrontend, "...");
                    } else {
                        strcpy(truncatedFrontend, selectedServer->frontend);
                    }

                    const char *messages[] = {
                        selectedServer->name,
                        selectedServer->game,
                        selectedServer->gameCRC,
                        selectedServer->core,
                        selectedServer->coreCRC,
                        truncatedFrontend,
                        selectedServer->ip,
                        selectedServer->retroarchVersion,
                    };

                    theme_renderExtendedTextbox(screen, 218, 72, messages, 18);
                    //draw gui elements
                                       
                    // box art
                    drawboxArt(screen, selectedServer->local.imgPath);
                    
                    if (strlen(selectedServer->local.romPath) == 0) {
                        drawgenericIcon(screen, ROM_MISSING, 555, 75);
                    } else if (strcmp(selectedServer->local.localRomCRC, selectedServer->gameCRC) == 0) {
                        drawgenericIcon(screen, ROM_OK, 555, 75);
                    } else {
                        drawgenericIcon(screen, ROM_BADCRC, 555, 75);
                    }
                    
                    // core icon (5 pixel space from rom icon)
                    if (strlen(selectedServer->local.corePath) == 0) {
                        drawgenericIcon(screen, CORE_MISSING, 555, 110);
                    } else {
                        const char* expectedCoreVersion = coreVersion(selectedServer->core);
                        if (expectedCoreVersion == NULL || strcmp(selectedServer->coreVersion, expectedCoreVersion) != 0) {
                            drawgenericIcon(screen, CORE_VER_MISMATCH, 555, 110);
                        } else {
                            drawgenericIcon(screen, CORE_FOUND, 555, 110);
                        }
                    }
                    
                    // ra version checker
                    if (strncmp(selectedServer->retroarchVersion, majorVersion, strlen(majorVersion)) == 0) {
                        drawgenericIcon(screen, RA_MATCH, 555, 145);
                    } else {
                        drawgenericIcon(screen, RA_MISMATCH, 555, 145);
                    }
                    
                    // relay icon
                    if (hasRelay(selectedServer->mitmIP)) {
                        drawgenericIcon(screen, RELAY_HASRELAY, 555, 180);
                    } else {
                        drawgenericIcon(screen, PASSWORD_RELAY_LOCK, 555, 180);
                    }
                    
                    // show password outline
                    if (selectedServer->hasPassword == 1) {
                        drawgenericIcon(screen, PASSWORD_HASPWD, 551, 70);
                    }
                    
                    // check if good match (rom matches, onion hosted server, no relay, ra version matches)
                    if (stringContains(selectedServer->name, "Onion")) {
                        if (!hasRelay(selectedServer->mitmIP)) {
                            if (strcmp(selectedServer->local.localRomCRC, selectedServer->gameCRC) == 0) {
                                if (strncmp(selectedServer->retroarchVersion, majorVersion, strlen(majorVersion)) == 0) {
                                    drawgenericIcon(screen, GOOD_MATCH, 551, 70);
                                }
                            }
                        }
                    }
                }
            }
            
            if (footer_changed) {
                theme_renderFooter(screen);
                theme_renderStandardHint(
                    screen, lang_get(LANG_SELECT, LANG_FALLBACK_SELECT),
                    required ? NULL : lang_get(LANG_BACK, LANG_FALLBACK_BACK));
            }

            if (footer_changed || list_changed)
                theme_renderFooterStatus(screen, list.active_pos + 1,
                                         list.item_count);

            if (header_changed || battery_changed)
                theme_renderHeaderBattery(screen, battery_getPercentage());

            footer_changed = false;
            header_changed = false;
            list_changed = false;
            battery_changed = false;
            refresh_head_foot = false;

            SDL_BlitSurface(screen, NULL, video, NULL);
            SDL_Flip(video);

            acc_ticks -= time_step;
        }
    }
    
    SDL_FillRect(video, NULL, 0);
    SDL_Flip(video);

    lang_free();


    log_output("Freeing lists...");
    list_free(&list);
    freeServerGlobal();
    clearImageCache();
    log_output(LOG_SUCCESS, "freed list");

    Mix_CloseAudio();

    resources_free();
    SDL_FreeSurface(screen);
    SDL_FreeSurface(video);

    SDL_Quit();
}
