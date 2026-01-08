#ifndef FASTBOOT_WRAPPER_H
#define FASTBOOT_WRAPPER_H

#include "common.h"

// Core fastboot functions
ProcessResult* RunFastbootCommand(const char* fastboot_path, const char* args[], int arg_count);

// Device management
ProcessResult* FastbootDevices(const char* fastboot_path);
ProcessResult* FastbootGetVar(const char* fastboot_path, const char* device_serial, const char* var_name);
ProcessResult* FastbootGetAllVars(const char* fastboot_path, const char* device_serial);

// Flashing operations
ProcessResult* FastbootFlash(const char* fastboot_path, const char* device_serial,
                             const char* partition, const char* image_path);
ProcessResult* FastbootErase(const char* fastboot_path, const char* device_serial,
                             const char* partition);
ProcessResult* FastbootFormat(const char* fastboot_path, const char* device_serial,
                              const char* partition, const char* fs_type);

// Bootloader management
ProcessResult* FastbootUnlock(const char* fastboot_path, const char* device_serial);
ProcessResult* FastbootLock(const char* fastboot_path, const char* device_serial);
ProcessResult* FastbootOemCommand(const char* fastboot_path, const char* device_serial,
                                  const char* oem_cmd);

// Reboot operations
ProcessResult* FastbootReboot(const char* fastboot_path, const char* device_serial,
                              const char* mode);
ProcessResult* FastbootRebootBootloader(const char* fastboot_path, const char* device_serial);

// Advanced operations
ProcessResult* FastbootActivateSlot(const char* fastboot_path, const char* device_serial,
                                    const char* slot);
ProcessResult* FastbootWipe(const char* fastboot_path, const char* device_serial,
                            const char* partition);

// Utility functions
int ParseFastbootDeviceList(const char* output, AdbDevice* devices, int max_devices);
char* ExtractFastbootVar(const char* output, const char* var_name);

#endif // FASTBOOT_WRAPPER_H
