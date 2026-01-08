#include "resource_extractor.h"
#include "utils.h"
#include <objbase.h>

// Create temporary directory with unique name
int CreateTempDirectory(char* temp_path_out, size_t temp_path_size) {
    if (!temp_path_out || temp_path_size == 0) return 0;

    // Get system temp path
    char temp_path[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_path);

    // Generate unique directory name using GUID
    GUID guid;
    CoCreateGuid(&guid);

    char guid_str[64];
    snprintf(guid_str, sizeof(guid_str),
             "%08lX%04X%04x%02X%02X%02X%02X%02X%02X%02X%02X",
             guid.Data1, guid.Data2, guid.Data3,
             guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
             guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

    snprintf(temp_path_out, temp_path_size, "%sadbtool_%s", temp_path, guid_str);

    // Create directory
    if (!CreateDirectoryA(temp_path_out, NULL)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            PrintError(ADB_ERROR_RESOURCE_EXTRACTION, "Failed to create temp directory");
            return 0;
        }
    }

    return 1;
}

// Extract a single resource from executable to file
int ExtractResource(HMODULE hModule, const char* resource_name, const char* output_path) {
    if (!resource_name || !output_path) return 0;

    // Find resource
    HRSRC hRes = FindResourceA(hModule, resource_name, RT_RCDATA);
    if (!hRes) {
        fprintf(stderr, "Failed to find resource: %s\n", resource_name);
        return 0;
    }

    // Load resource
    HGLOBAL hLoaded = LoadResource(hModule, hRes);
    if (!hLoaded) {
        fprintf(stderr, "Failed to load resource: %s\n", resource_name);
        return 0;
    }

    // Lock resource to get pointer
    void* pData = LockResource(hLoaded);
    if (!pData) {
        fprintf(stderr, "Failed to lock resource: %s\n", resource_name);
        return 0;
    }

    // Get resource size
    DWORD size = SizeofResource(hModule, hRes);
    if (size == 0) {
        fprintf(stderr, "Resource has zero size: %s\n", resource_name);
        return 0;
    }

    // Write to file
    FILE* fp = fopen(output_path, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to create file: %s\n", output_path);
        return 0;
    }

    size_t written = fwrite(pData, 1, size, fp);
    fclose(fp);

    if (written != size) {
        fprintf(stderr, "Failed to write complete file: %s\n", output_path);
        return 0;
    }

    return 1;
}

// Extract all required binaries to temp directory
int ExtractAllResources(const char* temp_dir, char* adb_path_out, char* fastboot_path_out) {
    if (!temp_dir) return 0;

    HMODULE hModule = GetModuleHandleA(NULL);
    if (!hModule) {
        PrintError(ADB_ERROR_RESOURCE_EXTRACTION, "Failed to get module handle");
        return 0;
    }

    // Extract ADB executable
    if (adb_path_out) {
        snprintf(adb_path_out, MAX_PATH, "%s/adb.exe", temp_dir);
        if (!ExtractResource(hModule, "ADB_EXE", adb_path_out)) {
            PrintError(ADB_ERROR_RESOURCE_EXTRACTION, "Failed to extract adb.exe");
            return 0;
        }
    }

    // Extract Fastboot
    if (fastboot_path_out) {
        snprintf(fastboot_path_out, MAX_PATH, "%s/fastboot.exe", temp_dir);
        if (!ExtractResource(hModule, "FASTBOOT_EXE", fastboot_path_out)) {
            PrintError(ADB_ERROR_RESOURCE_EXTRACTION, "Failed to extract fastboot.exe");
            return 0;
        }
    }

    // Extract DLLs
    char dll_path[MAX_PATH];
    snprintf(dll_path, sizeof(dll_path), "%s/AdbWinApi.dll", temp_dir);
    if (!ExtractResource(hModule, "ADBWINAPI_DLL", dll_path)) {
        PrintWarning("Failed to extract AdbWinApi.dll");
    }

    snprintf(dll_path, sizeof(dll_path), "%s/AdbWinUsbApi.dll", temp_dir);
    if (!ExtractResource(hModule, "ADBWINUSB_DLL", dll_path)) {
        PrintWarning("Failed to extract AdbWinUsbApi.dll");
    }

    // Extract optional utilities
    snprintf(dll_path, sizeof(dll_path), "%s/sqlite3.exe", temp_dir);
    ExtractResource(hModule, "SQLITE3_EXE", dll_path);

    snprintf(dll_path, sizeof(dll_path), "%s/7za.exe", temp_dir);
    ExtractResource(hModule, "SEVENZA_EXE", dll_path);

    snprintf(dll_path, sizeof(dll_path), "%s/mke2fs.exe", temp_dir);
    ExtractResource(hModule, "MKE2FS_EXE", dll_path);

    snprintf(dll_path, sizeof(dll_path), "%s/mke2fs.conf", temp_dir);
    ExtractResource(hModule, "MKE2FS_CONF", dll_path);

    return 1;
}

// Cleanup extracted files on exit
void CleanupResources(const char* temp_dir) {
    if (!temp_dir) return;

    // Build command to delete directory and all contents
    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\"", temp_dir);

    system(cmd);
}
