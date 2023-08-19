#ifndef MYRIAD_CORE_H
#define MYRIAD_CORE_H

char* searchcorePath(const char* base_path, const char* core_name);
char* searchromPathSQ(const char* base_path_rom, const char* rom_pattern);
char* searchromRecursive(const char* base_path_rom, const char* rom_pattern);
char* buildImgPath(const char* rom_path);

const char* coreVersion(const char* coreName);
void coreVersionIndexer();

#define MAX_CORES 200
#define BASE_PATH_CORE "/mnt/SDCARD/RetroArch/.retroarch/cores"
#define BASE_PATH_ROM "/mnt/SDCARD/Roms"

#endif // MYRIAD_CORE_H
