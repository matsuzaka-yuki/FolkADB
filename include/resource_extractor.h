#ifndef RESOURCE_EXTRACTOR_H
#define RESOURCE_EXTRACTOR_H

#include "common.h"

// Resource IDs
typedef enum {
    RESOURCE_ADB_EXE = 100,
    RESOURCE_ADBWINAPI_DLL = 101,
    RESOURCE_ADBWINUSB_DLL = 102,
    RESOURCE_FASTBOOT_EXE = 103,
    RESOURCE_MAKE_F2FS_EXE = 104,
    RESOURCE_MAKE_F2FS_CF_EXE = 105,
    RESOURCE_MKE2FS_EXE = 106,
    RESOURCE_MKE2FS_CONF = 107,
    RESOURCE_SQLITE3_EXE = 108,
    RESOURCE_7ZA_EXE = 109
} ResourceId;

// Functions
int CreateTempDirectory(char* temp_path_out, size_t temp_path_size);
int ExtractResource(HMODULE hModule, const char* resource_name, const char* output_path);
int ExtractAllResources(const char* temp_dir, char* adb_path_out, char* fastboot_path_out);
void CleanupResources(const char* temp_dir);

#endif // RESOURCE_EXTRACTOR_H
