#ifndef COMMON_H
#define COMMON_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <conio.h>

#define MAX_PATH 260
#define MAX_DEVICES 16
#define BUFFER_SIZE 4096
#define APP_VERSION "1.0.0"

// Error codes
typedef enum {
    ADB_SUCCESS = 0,
    ADB_ERROR_NO_DEVICE = 1,
    ADB_ERROR_MULTIPLE_DEVICES = 2,
    ADB_ERROR_DEVICE_OFFLINE = 3,
    ADB_ERROR_FILE_NOT_FOUND = 4,
    ADB_ERROR_PERMISSION_DENIED = 5,
    ADB_ERROR_CONNECTION_FAILED = 6,
    ADB_ERROR_RESOURCE_EXTRACTION = 7,
    ADB_ERROR_INVALID_COMMAND = 8,
    ADB_ERROR_TIMEOUT = 9,
    ADB_ERROR_FASTBOOT_FAILED = 10,
    ADB_ERROR_IMAGE_NOT_FOUND = 11,
    ADB_ERROR_UNLOCK_FAILED = 12,
    ADB_ERROR_INVALID_PARTITION = 13,
    ADB_ERROR_FLASH_FAILED = 14,
    ADB_ERROR_DEVICE_LOCKED = 15,
    ADB_ERROR_UNKNOWN = 255
} AdbErrorCode;

// Operation mode
typedef enum {
    MODE_ADB = 0,
    MODE_FASTBOOT = 1
} OperationMode;

// Device information structure
typedef struct {
    char serial_id[256];
    char model[256];
    char device_name[256];
    char status[64];  // "device", "offline", "unauthorized"
    char android_version[64];
    char api_level[16];
} AdbDevice;

// Process execution result
typedef struct {
    char* stdout_data;
    char* stderr_data;
    size_t stdout_size;
    size_t stderr_size;
    int exit_code;
} ProcessResult;

// Application state
typedef struct {
    char adb_path[MAX_PATH];
    char fastboot_path[MAX_PATH];
    char temp_dir[MAX_PATH];
    AdbDevice devices[MAX_DEVICES];
    AdbDevice fastboot_devices[MAX_DEVICES];
    int device_count;
    int fastboot_device_count;
    int current_device_index;
    int current_fastboot_device_index;
    OperationMode current_mode;
    int verbose;
} AppState;

// Progress callback type
typedef void (*ProgressCallback)(const char* filename, size_t current, size_t total);

// Utility macros
#define SAFE_FREE(ptr) if(ptr) { free(ptr); ptr = NULL; }
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#endif // COMMON_H
