#ifndef ADB_WRAPPER_H
#define ADB_WRAPPER_H

#include "common.h"

// Core ADB functions
ProcessResult* RunAdbCommand(const char* adb_path, const char* args[], int arg_count);
void FreeProcessResult(ProcessResult* result);

// ADB command wrappers
ProcessResult* AdbDevices(const char* adb_path);
ProcessResult* AdbShellCommand(const char* adb_path, const char* device_serial, const char* command);
ProcessResult* AdbGetProp(const char* adb_path, const char* device_serial, const char* prop);
ProcessResult* AdbPushFile(const char* adb_path, const char* device_serial,
                           const char* local_path, const char* remote_path);
ProcessResult* AdbPullFile(const char* adb_path, const char* device_serial,
                           const char* remote_path, const char* local_path);
ProcessResult* AdbInstallApk(const char* adb_path, const char* device_serial, const char* apk_path);
ProcessResult* AdbUninstallPackage(const char* adb_path, const char* device_serial, const char* package);
ProcessResult* AdbReboot(const char* adb_path, const char* device_serial, const char* mode);

// Utility functions
int ParseDeviceList(const char* output, AdbDevice* devices, int max_devices);
char* ExtractPropValue(const char* output, const char* prop_name);

#endif // ADB_WRAPPER_H
