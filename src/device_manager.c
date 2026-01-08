#include "device_manager.h"
#include "adb_wrapper.h"
#include "fastboot_wrapper.h"
#include "utils.h"
#include <stdio.h>
#include <time.h>

// Refresh device list from ADB
int RefreshDeviceList(AppState* state) {
    if (!state) return 0;

    // Save current device serial to try to maintain selection
    char saved_serial[256] = "";
    if (state->current_device_index >= 0 &&
        state->current_device_index < state->device_count) {
        strncpy(saved_serial, state->devices[state->current_device_index].serial_id,
                sizeof(saved_serial) - 1);
    }

    ProcessResult* result = AdbDevices(state->adb_path);
    if (!result) {
        PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to get device list");
        return 0;
    }

    int count = ParseDeviceList(result->stdout_data, state->devices, MAX_DEVICES);
    state->device_count = count;

    FreeProcessResult(result);

    // Try to restore selection by serial number
    if (strlen(saved_serial) > 0) {
        for (int i = 0; i < count; i++) {
            if (strcmp(state->devices[i].serial_id, saved_serial) == 0) {
                state->current_device_index = i;
                return count;
            }
        }
    }

    // If we couldn't restore selection, reset to first device or -1
    if (state->current_device_index >= count) {
        state->current_device_index = (count > 0) ? 0 : -1;
    }

    return count;
}

// Get device count
int GetDeviceCount(const AppState* state) {
    return state ? state->device_count : 0;
}

// Get currently selected device
AdbDevice* GetSelectedDevice(const AppState* state) {
    if (!state || state->current_device_index < 0 || state->current_device_index >= state->device_count) {
        return NULL;
    }
    return &state->devices[state->current_device_index];
}

// Select device by index
int SelectDevice(AppState* state, int index) {
    if (!state || index < 0 || index >= state->device_count) {
        return 0;
    }

    state->current_device_index = index;

    // Get detailed device info
    GetDeviceInfo(state, index);

    return 1;
}

// Select device by serial number
int SelectDeviceBySerial(AppState* state, const char* serial) {
    if (!state || !serial) return 0;

    for (int i = 0; i < state->device_count; i++) {
        if (strcmp(state->devices[i].serial_id, serial) == 0) {
            state->current_device_index = i;
            GetDeviceInfo(state, i);
            return 1;
        }
    }

    return 0;
}

// Get detailed device information
int GetDeviceInfo(AppState* state, int device_index) {
    if (!state || device_index < 0 || device_index >= state->device_count) {
        return 0;
    }

    AdbDevice* device = &state->devices[device_index];

    // Get Android version
    ProcessResult* result = AdbGetProp(state->adb_path, device->serial_id, "ro.build.version.release");
    if (result && result->stdout_data) {
        TrimString(result->stdout_data);
        strncpy(device->android_version, result->stdout_data, sizeof(device->android_version) - 1);
    }
    FreeProcessResult(result);

    // Get API level
    result = AdbGetProp(state->adb_path, device->serial_id, "ro.build.version.sdk");
    if (result && result->stdout_data) {
        TrimString(result->stdout_data);
        strncpy(device->api_level, result->stdout_data, sizeof(device->api_level) - 1);
    }
    FreeProcessResult(result);

    return 1;
}

// Print device list
void PrintDeviceList(const AppState* state) {
    if (!state) return;

    printf("\n");
    printf("========================================\n");
    printf("         Connected Devices (%d)\n", state->device_count);
    printf("========================================\n");

    if (state->device_count == 0) {
        printf("No devices connected.\n");
        printf("\nPlease make sure:\n");
        printf("- USB debugging is enabled on your device\n");
        printf("- Device is connected via USB\n");
        printf("- You have authorized this computer on the device\n");
    } else {
        for (int i = 0; i < state->device_count; i++) {
            AdbDevice* dev = &state->devices[i];
            printf("[%d] %s", i, dev->serial_id);

            if (i == state->current_device_index) {
                printf(" [SELECTED]");
            }

            printf("\n");
            printf("    Model: %s\n", strlen(dev->model) > 0 ? dev->model : "Unknown");
            printf("    Device: %s\n", strlen(dev->device_name) > 0 ? dev->device_name : "Unknown");
            printf("    Status: %s\n", dev->status);

            if (strlen(dev->android_version) > 0) {
                printf("    Android: %s (API %s)\n", dev->android_version, dev->api_level);
            }

            printf("\n");
        }
    }

    printf("========================================\n");
}

// Wait for device connection (with timeout)
int WaitForDeviceConnection(AppState* state, int timeout_seconds) {
    if (!state) return 0;

    printf("\nWaiting for device connection...\n");
    printf("Press Ctrl+C to cancel\n\n");

    time_t start_time = time(NULL);

    while (1) {
        int count = RefreshDeviceList(state);

        if (count > 0) {
            printf("\nDevice connected!\n");
            return 1;
        }

        if (timeout_seconds > 0) {
            time_t elapsed = (int)(time(NULL) - start_time);
            if (elapsed >= timeout_seconds) {
                printf("\nTimeout waiting for device.\n");
                return 0;
            }

            printf("\rWaiting... %d seconds elapsed", (int)elapsed);
        }

        Sleep(1000); // Wait 1 second
    }

    return 0;
}

// ============================================================================
// Fastboot Device Management Functions
// ============================================================================

// Refresh fastboot device list
int RefreshFastbootDeviceList(AppState* state) {
    if (!state) return 0;

    // Save current device serial to try to maintain selection
    char saved_serial[256] = "";
    if (state->current_fastboot_device_index >= 0 &&
        state->current_fastboot_device_index < state->fastboot_device_count) {
        strncpy(saved_serial, state->fastboot_devices[state->current_fastboot_device_index].serial_id,
                sizeof(saved_serial) - 1);
    }

    ProcessResult* result = FastbootDevices(state->fastboot_path);
    if (!result) {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to get fastboot device list");
        return 0;
    }

    int count = ParseFastbootDeviceList(result->stdout_data, state->fastboot_devices, MAX_DEVICES);
    state->fastboot_device_count = count;

    FreeProcessResult(result);

    // Try to restore selection by serial number
    if (strlen(saved_serial) > 0) {
        for (int i = 0; i < count; i++) {
            if (strcmp(state->fastboot_devices[i].serial_id, saved_serial) == 0) {
                state->current_fastboot_device_index = i;
                return count;
            }
        }
    }

    // If we couldn't restore selection, reset to first device or -1
    if (state->current_fastboot_device_index >= count) {
        state->current_fastboot_device_index = (count > 0) ? 0 : -1;
    }

    return count;
}

// Get fastboot device count
int GetFastbootDeviceCount(const AppState* state) {
    return state ? state->fastboot_device_count : 0;
}

// Get selected fastboot device
AdbDevice* GetSelectedFastbootDevice(const AppState* state) {
    if (!state || state->current_fastboot_device_index < 0 ||
        state->current_fastboot_device_index >= state->fastboot_device_count) {
        return NULL;
    }
    return &state->fastboot_devices[state->current_fastboot_device_index];
}

// Select fastboot device by index
int SelectFastbootDevice(AppState* state, int index) {
    if (!state || index < 0 || index >= state->fastboot_device_count) {
        return 0;
    }

    state->current_fastboot_device_index = index;
    return 1;
}

// Select fastboot device by serial
int SelectFastbootDeviceBySerial(AppState* state, const char* serial) {
    if (!state || !serial) return 0;

    for (int i = 0; i < state->fastboot_device_count; i++) {
        if (strcmp(state->fastboot_devices[i].serial_id, serial) == 0) {
            state->current_fastboot_device_index = i;
            return 1;
        }
    }

    return 0;
}

// Print fastboot device list
void PrintFastbootDeviceList(const AppState* state) {
    if (!state) return;

    printf("\n");
    printf("========================================\n");
    printf("       Fastboot Devices (%d)\n", state->fastboot_device_count);
    printf("========================================\n");

    if (state->fastboot_device_count == 0) {
        printf("No fastboot devices connected.\n");
        printf("\nPlease make sure:\n");
        printf("- Device is in fastboot mode\n");
        printf("- Device is connected via USB\n");
        printf("- Fastboot drivers are installed\n");
    } else {
        for (int i = 0; i < state->fastboot_device_count; i++) {
            AdbDevice* dev = &state->fastboot_devices[i];
            printf("[%d] %s", i, dev->serial_id);

            if (i == state->current_fastboot_device_index) {
                printf(" [SELECTED]");
            }

            printf("\n");
            printf("    Status: %s\n", dev->status);
            printf("\n");
        }
    }

    printf("========================================\n");
}

// ============================================================================
// Auto-Monitoring and Mode Switching
// ============================================================================

static volatile int g_monitoring_enabled = 0;
static AppState* g_monitor_state = NULL;

// Callback function to notify CLI to refresh prompt
typedef void (*PromptRefreshCallback)(void);
static PromptRefreshCallback g_prompt_refresh_callback = NULL;

void SetPromptRefreshCallback(PromptRefreshCallback callback) {
    g_prompt_refresh_callback = callback;
}

// Thread handle for monitoring
#ifdef _WIN32
#include <windows.h>
static HANDLE g_monitor_thread = NULL;
#endif

// Monitor thread function
#ifdef _WIN32
DWORD WINAPI MonitorThread(LPVOID lpParam) {
    AppState* state = (AppState*)lpParam;
    
    while (g_monitoring_enabled) {
        CheckDeviceMode(state);
        Sleep(3000); // Wait 3 seconds
    }
    
    return 0;
}
#endif

// Start device monitoring
void StartDeviceMonitoring(AppState* state) {
    if (!state || g_monitoring_enabled) {
        return;
    }
    
    g_monitor_state = state;
    g_monitoring_enabled = 1;
    
#ifdef _WIN32
    // Create monitoring thread
    g_monitor_thread = CreateThread(
        NULL,                   // Default security attributes
        0,                      // Default stack size
        MonitorThread,          // Thread function
        state,                  // Parameter to thread function
        0,                      // Default creation flags
        NULL                    // Don't need thread identifier
    );
    
    if (g_monitor_thread == NULL) {
        printf("Failed to start device monitoring thread.\n");
        g_monitoring_enabled = 0;
        return;
    }
    
    printf("Device monitoring started (checking every 3 seconds)...\n");
#else
    printf("Device monitoring not supported on this platform.\n");
    g_monitoring_enabled = 0;
#endif
}

// Stop device monitoring
void StopDeviceMonitoring(void) {
    if (!g_monitoring_enabled) {
        return;
    }
    
    g_monitoring_enabled = 0;
    
#ifdef _WIN32
    if (g_monitor_thread != NULL) {
        // Wait for thread to finish (with timeout)
        WaitForSingleObject(g_monitor_thread, 5000);
        CloseHandle(g_monitor_thread);
        g_monitor_thread = NULL;
    }
#endif
    
    g_monitor_state = NULL;
    printf("Device monitoring stopped.\n");
}

// Check device mode and auto-switch
int CheckDeviceMode(AppState* state) {
    if (!state) return 0;

    // Refresh both ADB and fastboot device lists
    int old_adb_count = state->device_count;
    int old_fastboot_count = state->fastboot_device_count;

    RefreshDeviceList(state);
    RefreshFastbootDeviceList(state);

    int adb_count = state->device_count;
    int fastboot_count = state->fastboot_device_count;

    // Auto-switch logic
    int mode_changed = 0;
    int needs_refresh = 0;

    // Priority: fastboot > ADB (fastboot is more privileged/lower-level)
    // If fastboot devices detected, switch to fastboot mode
    if (fastboot_count > 0 && state->current_mode != MODE_FASTBOOT) {
        printf("\n[Auto-switch] Fastboot device detected, switching to fastboot mode...\n");
        state->current_mode = MODE_FASTBOOT;

        // Auto-select first fastboot device
        if (state->current_fastboot_device_index < 0) {
            SelectFastbootDevice(state, 0);
            AdbDevice* dev = GetSelectedFastbootDevice(state);
            if (dev) {
                printf("Auto-selected fastboot device: %s\n", dev->serial_id);
            }
        }
        mode_changed = 1;
        needs_refresh = 1;
    }

    // If only ADB devices detected (and no fastboot), switch to ADB mode
    // Changed to independent 'if' instead of 'else if'
    if (adb_count > 0 && fastboot_count == 0 && state->current_mode != MODE_ADB) {
        printf("\n[Auto-switch] ADB device detected, switching to ADB mode...\n");
        state->current_mode = MODE_ADB;

        // Auto-select first ADB device
        if (state->current_device_index < 0) {
            SelectDevice(state, 0);
            AdbDevice* dev = GetSelectedDevice(state);
            if (dev) {
                printf("Auto-selected ADB device: %s\n", dev->serial_id);
            }
        }
        mode_changed = 1;
        needs_refresh = 1;
    }

    // If no devices at all, reset to ADB mode
    if (adb_count == 0 && fastboot_count == 0 && state->current_mode != MODE_ADB) {
        state->current_mode = MODE_ADB;
        mode_changed = 1;
        needs_refresh = 1;
    }

    // Auto-select device if none selected in current mode
    if (!mode_changed) {
        // In fastboot mode with devices but no selection
        if (state->current_mode == MODE_FASTBOOT && fastboot_count > 0 &&
            state->current_fastboot_device_index < 0) {
            SelectFastbootDevice(state, 0);
            AdbDevice* dev = GetSelectedFastbootDevice(state);
            if (dev) {
                printf("\n[Auto-select] Fastboot device: %s\n", dev->serial_id);
                needs_refresh = 1;
            }
        }
        // In ADB mode with devices but no selection
        else if (state->current_mode == MODE_ADB && adb_count > 0 &&
                 state->current_device_index < 0) {
            SelectDevice(state, 0);
            AdbDevice* dev = GetSelectedDevice(state);
            if (dev) {
                printf("\n[Auto-select] ADB device: %s\n", dev->serial_id);
                needs_refresh = 1;
            }
        }
    }

    // Notify of device changes
    if (!mode_changed) {
        if (adb_count != old_adb_count || fastboot_count != old_fastboot_count) {
            if (adb_count > 0 && fastboot_count > 0) {
                printf("\n[Monitor] %d ADB device(s), %d fastboot device(s) connected\n",
                       adb_count, fastboot_count);
            } else if (adb_count > 0) {
                printf("\n[Monitor] %d ADB device(s) connected\n", adb_count);
            } else if (fastboot_count > 0) {
                printf("\n[Monitor] %d fastboot device(s) connected\n", fastboot_count);
            } else {
                printf("\n[Monitor] No devices connected\n");
            }
            needs_refresh = 1;
        }
    }

    // Trigger prompt refresh if needed
    if (needs_refresh && g_prompt_refresh_callback) {
        g_prompt_refresh_callback();
    }

    return mode_changed;
}

