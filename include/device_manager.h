#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "common.h"

// Device management functions
int RefreshDeviceList(AppState* state);
int GetDeviceCount(const AppState* state);
AdbDevice* GetSelectedDevice(const AppState* state);
int SelectDevice(AppState* state, int index);
int SelectDeviceBySerial(AppState* state, const char* serial);
int GetDeviceInfo(AppState* state, int device_index);
void PrintDeviceList(const AppState* state);
int WaitForDeviceConnection(AppState* state, int timeout_seconds);

// Fastboot device management functions
int RefreshFastbootDeviceList(AppState* state);
int GetFastbootDeviceCount(const AppState* state);
AdbDevice* GetSelectedFastbootDevice(const AppState* state);
int SelectFastbootDevice(AppState* state, int index);
int SelectFastbootDeviceBySerial(AppState* state, const char* serial);
void PrintFastbootDeviceList(const AppState* state);

// Auto-monitoring and mode switching
void StartDeviceMonitoring(AppState* state);
void StopDeviceMonitoring(void);
int CheckDeviceMode(AppState* state);
void SetPromptRefreshCallback(void (*callback)(void));

#endif // DEVICE_MANAGER_H
