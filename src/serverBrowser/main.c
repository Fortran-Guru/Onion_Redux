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
#include "theme/sound.h"
#include "theme/theme.h"
#include "utils/keystate.h"
#include "utils/log.h"
#include "utils/sdl_init.h"
#include "utils/str.h"

//local
#include "misc_utils.h"
#include "myriad_core.h"
#include "parse_json.h"
#include "gui_draw.h"
#include "cache_local.h"
#include "net.h"

#define FRAMES_PER_SECOND 60

/*
TODO
FINISH LOGGING FUNCTIONS DESCRIP
FINISH LAN QUERY (ADD TO STRUCT, ADD TO LIST)
*/
// asdasdsdasd

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
    
    miscLogOutput(__func__, "Attempting to join server: %s at IP %s with port %d", server->name, server->ip, server->port);
    miscLogOutput(__func__, "Hosts rom is: %s with CRC of %s, your rom is: %s", server->game, server->gameCRC, server->local.localRomCRC);
    miscLogOutput(__func__, "Hosts core is: %s with CRC/Commit of %s", server->core, server->coreCRC);
    miscLogOutput(__func__, "Hosts RetroArch version is %s", server->retroarchVersion);
    miscLogOutput(__func__, "Hosts frontend (OS) is %s", server->frontend);
    miscLogOutput(__func__, "Hosts server has relay? %s with hostname of %s and port of %d", server->mitmSession, server->mitmIP, server->mitmPort); 
    miscLogOutput(__func__, "Hosts server has password/is private? %d", server->hasPassword);
    miscLogOutput(__func__, "Hosts server is connectable? %d", server->connectable);
    miscLogOutput(__func__, "Local rom path is: %s", server->local.romPath);
    
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

    if (server->hostMethod == 3) {
        showDialog("Connection Error", "Relay servers not currently supported. \n \n Please try another server.");
    }    
    
    if (startRetroarch) {      
        char serverIP[32];
        char serverPort[12];
        
        if (server->hostMethod == 3) {
            strcpy(serverIP, server->mitmIP);
            sprintf(serverPort, "%d", server->mitmPort);
        } else {
            strcpy(serverIP, server->ip);
            sprintf(serverPort, "%d", server->port);
        }
        
        snprintf(cmd_line, sizeof(cmd_line), 
                 "HOME=/mnt/SDCARD/RetroArch cd /mnt/SDCARD/RetroArch && ./retroarch -C %s --port=%s -v -L \"%s\" \"%s\"",
                 serverIP, serverPort, server->local.corePath, server->local.romPath);

        miscLogOutput(__func__, "Prepared command: %s", cmd_line);

        printf("CMD_OUTPUT: %s\n", cmd_line); // leave me as printf.
        quit = true;
    }
    firstDialog = false;
}


int main(int argc, char *argv[])
{
    
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    char title_str[STR_MAX] = "Onion Server Browser";
    char *majorVersion = miscGetRAMajorVersion();
    
    int selected = 0;
    int i;
    bool required = false;
    
    SDL_InitDefault(true);
    settings_load();
    lang_load();
    serverCountGlobal = 0;
           
    if (!majorVersion) {
        miscLogOutput(__func__, "Error retrieving the major version.\n");
        return 1;
    }
    
    theme_renderFooter(screen);
    theme_renderHeaderBattery(screen, battery_getPercentage());
    theme_renderDialog(screen, "Searching...", "Searching for servers...", true); // build a landing page before we cache, find, build the list.

    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
            
    if (netWlan0Exists()) {
        pthread_t queryThread;
        pthread_create(&queryThread, NULL, netQueryLanThread, NULL);
        bool serverIsReachable = netIsServerReachable("34.102.164.250"); // RA lobby server IP address 34.102.164.250, test no connection with 123.231.123.231

        if(serverIsReachable) {
            miscLogOutput(__func__, "Reachable! Sending data request");
            parseRetrieveData();
            myriadCoreVersionIndexer();
            miscLogOutput(__func__, "Building server list");
        } else {
            miscLogOutput(__func__, "Unable to reach server, cannot continue");
            lobbyReachable = false;
            strncpy(title_str, "No Connection", STR_MAX);
        }
    } else {
        miscLogOutput(__func__, "Wifi disabled? wlan0 not found");
        strncpy(title_str, "Wifi Disabled", STR_MAX);
        wifiDisabled = true;
    }
    
    int battery_percentage = battery_getPercentage();
    List list = list_create(serverCountGlobal, LIST_TINY);
    
    // miscPrintAllServers(); //debug
      
    if (serverCountGlobal > 0 && serversGlobal != NULL) {
        for (i = 0; i < serverCountGlobal; i++) {
            serversGlobal[i].local.romPath[0] = '\0';
            serversGlobal[i].local.corePath[0] = '\0';
            serversGlobal[i].local.imgPath[0] = '\0';
            serversGlobal[i].local.localRomCRC[0] = '\0';
            
            // checks ping, but most servers don't actually reply - will integrate this later
            
            // double latency = netGetLatency(serversGlobal[i].ip);
            // if (latency >= 0) {
                // miscLogOutput(__func__, "Estimated latency to %s: %.6f seconds", serversGlobal[i].ip, latency);
            // } else {
                // miscLogOutput(__func__, "Failed to estimate latency to %s.", serversGlobal[i].ip);
            // }
            
            char prefix[50] = "";
            if(serversGlobal[i].connectable == 0) {
                strcpy(prefix, "[Not connectable] ");
            } else {
                if(serversGlobal[i].hasPassword != 0) {
                    strcpy(prefix, "[Passworded] ");
                }
                
                char* found_rom_path = NULL;
                bool found_in_cache = false;

                if (serversGlobal[i].gameCRC) {
                    found_rom_path = cacheLookupRomLocal(serversGlobal[i].gameCRC, true);
                    if (found_rom_path) found_in_cache = true;
                }
                if (!found_rom_path) {
                    found_rom_path = cacheLookupRomLocal(serversGlobal[i].game, false);
                    if (found_rom_path) found_in_cache = true;
                }
                if (!found_rom_path) {
                    found_rom_path = myriadSearchRomPathSQ(BASE_PATH_ROM, serversGlobal[i].game);
                    if (!found_rom_path) {
                        found_rom_path = myriadSearchRomRecursive(BASE_PATH_ROM, serversGlobal[i].game);
                    }
                }

                if (found_rom_path) {
                    strncpy(serversGlobal[i].local.romPath, found_rom_path, sizeof(serversGlobal[i].local.romPath) - 1);
                    serversGlobal[i].local.romPath[sizeof(serversGlobal[i].local.romPath) - 1] = '\0';
                }

                char* found_core_path = myriadSearchCorePath(BASE_PATH_CORE, serversGlobal[i].core);
                if (found_core_path) {
                    strncpy(serversGlobal[i].local.corePath, found_core_path, sizeof(serversGlobal[i].local.corePath) - 1);
                    serversGlobal[i].local.corePath[sizeof(serversGlobal[i].local.corePath) - 1] = '\0';
                }
                
                if (serversGlobal[i].local.romPath[0] != '\0') {
                    if (miscHasFileExt(serversGlobal[i].local.romPath, ".zip") || miscHasFileExt(serversGlobal[i].local.romPath, ".7z")) {
                        char *tempCRC = miscGet7zCRC(serversGlobal[i].local.romPath);
                        if (tempCRC) {
                            strncpy(serversGlobal[i].local.localRomCRC, tempCRC, sizeof(serversGlobal[i].local.localRomCRC) - 1);
                            serversGlobal[i].local.localRomCRC[sizeof(serversGlobal[i].local.localRomCRC) - 1] = '\0';
                            free(tempCRC);
                        }
                    } else {
                        unsigned long rom_crc = miscCalculateCRC32(serversGlobal[i].local.romPath);
                        snprintf(serversGlobal[i].local.localRomCRC, sizeof(serversGlobal[i].local.localRomCRC), "%08lX", rom_crc);
                    }

                    if (!found_in_cache) {
                        cacheAddRom(serversGlobal[i].game, found_rom_path, serversGlobal[i].local.localRomCRC);

                        char* imgFilePath = myriadBuildImgPath(serversGlobal[i].local.romPath);
                        if (imgFilePath) {
                            strncpy(serversGlobal[i].local.imgPath, imgFilePath, sizeof(serversGlobal[i].local.imgPath) - 1);
                            serversGlobal[i].local.imgPath[sizeof(serversGlobal[i].local.imgPath) - 1] = '\0';
                        }
                    }
                } else {
                    miscLogOutput(__func__, "No rom path available for server: %s and rom: %s \n", serversGlobal[i].name, serversGlobal[i].game);
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
    
    // miscPrintAllServers(); //debug
       
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
                    miscFreeServerGlobal();
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
                    char gameCRCPrefixed[32];
                    char raVersionPrefixed[20];
                    char truncatedCore[14];

                    sprintf(gameCRCPrefixed, "CRC: %s", selectedServer->gameCRC);
                    sprintf(raVersionPrefixed, "RA Ver: %s", selectedServer->retroarchVersion);

                    if (strlen(selectedServer->frontend) > 10) {
                        strncpy(truncatedFrontend, selectedServer->frontend, 10);
                        truncatedFrontend[10] = '\0';
                        strcat(truncatedFrontend, "..");
                    } else {
                        strcpy(truncatedFrontend, selectedServer->frontend);
                    }

                    // Truncating the core
                    if (strlen(selectedServer->core) > 13) {
                        strncpy(truncatedCore, selectedServer->core, 13);
                        truncatedCore[13] = '\0';
                    } else {
                        strcpy(truncatedCore, selectedServer->core);
                    }

                    const char *messages[] = {
                        selectedServer->name,
                        selectedServer->game,
                        gameCRCPrefixed,
                        truncatedCore,
                        selectedServer->coreVersion,
                        truncatedFrontend,
                        selectedServer->ip,
                        raVersionPrefixed,
                    };
                    
                    //draw gui elements
                    theme_renderExtendedTextbox(screen, 218, 72, messages, 18);
                    
                                       
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
                        const char* expectedCoreVersion = myriadReturnCoreVer(selectedServer->core);
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
                    if (netHasRelay(selectedServer->mitmIP)) {
                        drawgenericIcon(screen, RELAY_HAS_RELAY, 555, 180);
                    } else {
                        drawgenericIcon(screen, PASSWORD_RELAY_LOCK, 555, 180);
                    }
                    
                    // show password outline
                    if (selectedServer->hasPassword == 1) {
                        drawgenericIcon(screen, PASSWORD_HASPWD, 551, 70);
                    }
                    
                    // check if good match (rom matches, onion hosted server, no relay, ra version matches)
                    if (miscStringContains(selectedServer->name, "Onion")) {
                        if (!netHasRelay(selectedServer->mitmIP)) {
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

    // bit of cleanup 
    miscLogOutput(__func__, "Cleaning up");
    lang_free();
    list_free(&list);
    miscFreeServerGlobal();
    cacheClearImageCache();
    Mix_CloseAudio();
    resources_free();
    SDL_FreeSurface(screen);
    SDL_FreeSurface(video);
    miscLogOutput(__func__, "Cleanup complete");
    SDL_Quit();
}
