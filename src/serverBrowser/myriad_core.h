#ifndef MYRIAD_CORE_H
#define MYRIAD_CORE_H

char* myriadSearchCorePath(const char* base_path, const char* core_name);
char* myriadSearchRomPathSQ(const char* base_path_rom, const char* rom_pattern);
char* myriadSearchRomRecursive(const char* base_path_rom, const char* rom_pattern);
char* myriadBuildImgPath(const char* rom_path);

const char* myriadReturnCoreVer(const char* coreName);
void myriadCoreVersionIndexer();

#define MAX_CORES 200
#define BASE_PATH_CORE "/mnt/SDCARD/RetroArch/.retroarch/cores"
#define BASE_PATH_ROM "/mnt/SDCARD/Roms"

#endif // MYRIAD_CORE_H
