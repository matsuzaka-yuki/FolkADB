#ifndef FASTBOOT_MANAGER_H
#define FASTBOOT_MANAGER_H

#include "common.h"

// High-level fastboot operations with safety checks and user interaction

// Flashing operations
int FlashImage(AppState* state, const char* partition, const char* image_path);
int ErasePartition(AppState* state, const char* partition);
int FormatPartition(AppState* state, const char* partition, const char* fs_type);

// Bootloader operations
int UnlockBootloader(AppState* state);
int LockBootloader(AppState* state);
int ExecuteOemCommand(AppState* state, const char* oem_cmd);

// Device operations
int RebootFastbootDevice(AppState* state, const char* mode);
int GetFastbootVar(AppState* state, const char* var_name);
int ActivateFastbootSlot(AppState* state, const char* slot);
int WipeFastbootPartition(AppState* state, const char* partition);

// Device info
int ShowFastbootDeviceInfo(AppState* state);

#endif // FASTBOOT_MANAGER_H
