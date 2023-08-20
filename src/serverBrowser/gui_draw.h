#ifndef GUI_DRAW_H
#define GUI_DRAW_H

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#define RESOURCES_DIR "/mnt/SDCARD/App/ServerBrowser/res/"

// roms
#define ROM_OK RESOURCES_DIR "srvr_brwsr_r_ok.png"
#define ROM_BADCRC RESOURCES_DIR "srvr_brwsr_r_badcrc.png"
#define ROM_MISSING RESOURCES_DIR "srvr_brwsr_r_missing.png"

// core
#define CORE_FOUND RESOURCES_DIR "srvr_brwsr_c_found.png"
#define CORE_VER_MISMATCH RESOURCES_DIR "srvr_brwsr_c_mismatch.png"
#define CORE_MISSING RESOURCES_DIR "srvr_brwsr_c_missing.png"

// RA version
#define RA_MATCH RESOURCES_DIR "srvr_brwsr_r_raver_ok.png"
#define RA_MISMATCH RESOURCES_DIR "srvr_brwsr_r_raver_bad.png"

// relayserver
#define RELAY_miscHasRelay RESOURCES_DIR "srvr_brwsr_rl_hasrelay.png"
#define RELAY_NORELAY RESOURCES_DIR "srvr_brwsr_rl_lock.png"

// pwd
#define PASSWORD_HASPWD RESOURCES_DIR "srvr_brwsr_lock.png"
#define PASSWORD_RELAY_LOCK RESOURCES_DIR "srvr_brwsr_rl_lock.png"

// good match (indicates rom, core, ra ver and a miyoo host)
#define GOOD_MATCH RESOURCES_DIR "srvr_brwsr_good_match.png"

void drawboxArt(SDL_Surface* screen, const char* rom_name);
void drawgenericIcon(SDL_Surface* screen, const char* img_path, int x, int y);

#endif // GUI_DRAW_H