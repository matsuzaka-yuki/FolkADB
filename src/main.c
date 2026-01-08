#include "common.h"
#include "resource_extractor.h"
#include "device_manager.h"
#include "cli.h"
#include "utils.h"
#include "file_transfer.h"
#include "adb_wrapper.h"
#include "module_installer.h"

// Global state for cleanup
static AppState g_state = {0};

// Cleanup function called on exit
static void Cleanup(void) {
    printf("\nCleaning up...\n");

    // Stop device monitoring
    StopDeviceMonitoring();

    // Cleanup extracted resources
    if (strlen(g_state.temp_dir) > 0) {
        CleanupResources(g_state.temp_dir);
    }
}

// Initialize application
static int Initialize(AppState* state) {
    // Set console output to UTF-8 to fix encoding issues with adb shell output
    SetConsoleOutputCP(CP_UTF8);

    // Set console window title
    char title_buffer[64];
    snprintf(title_buffer, sizeof(title_buffer), "FolkAdb v%s", APP_VERSION);
    SetConsoleTitleA(title_buffer);

    memset(state, 0, sizeof(AppState));
    state->current_device_index = -1;
    state->current_fastboot_device_index = -1;
    state->current_mode = MODE_ADB;

    // Show banner
    ShowBanner();

    // Extract embedded resources
    printf("Extracting embedded resources...\n");
    if (!CreateTempDirectory(state->temp_dir, sizeof(state->temp_dir))) {
        PrintError(ADB_ERROR_RESOURCE_EXTRACTION, "Failed to create temp directory");
        return 0;
    }

    if (!ExtractAllResources(state->temp_dir, state->adb_path, state->fastboot_path)) {
        PrintError(ADB_ERROR_RESOURCE_EXTRACTION, "Failed to extract resources");
        return 0;
    }

    printf("Resources extracted to: %s\n", state->temp_dir);
    printf("ADB path: %s\n", state->adb_path);
    printf("Fastboot path: %s\n", state->fastboot_path);

    // Register cleanup function
    atexit(Cleanup);

    return 1;
}

// Auto-connect to device
static int AutoConnect(AppState* state) {
    printf("\nScanning for devices...\n");

    // Check both ADB and fastboot devices
    RefreshDeviceList(state);
    RefreshFastbootDeviceList(state);

    int adb_count = state->device_count;
    int fastboot_count = state->fastboot_device_count;
    int total_count = adb_count + fastboot_count;

    if (total_count == 0) {
        printf("\nNo devices found.\n");
        printf("Please:\n");
        printf("1. Enable USB debugging on your Android device\n");
        printf("2. Connect your device via USB\n");
        printf("3. Authorize this computer on your device\n");
        printf("\nType 'devices' to rescan or 'help' for other commands.\n");
        return 0;
    }

    // Priority: fastboot > ADB
    if (fastboot_count > 0) {
        // Auto-switch to fastboot mode
        state->current_mode = MODE_FASTBOOT;
        SelectFastbootDevice(state, 0);
        AdbDevice* dev = GetSelectedFastbootDevice(state);
        printf("\nFastboot device detected: %s\n", dev->serial_id);
        printf("Auto-switched to fastboot mode.\n");
        return 1;
    }
    else if (adb_count == 1) {
        // Auto-select single ADB device
        SelectDevice(state, 0);
        AdbDevice* dev = GetSelectedDevice(state);
        printf("\nAutomatically connected to: %s\n", dev->serial_id);

        if (strlen(dev->android_version) > 0) {
            printf("Device: %s, Android %s (API %s)\n",
                   dev->model,
                   dev->android_version,
                   dev->api_level);
        }
        return 1;
    }
    else if (adb_count > 1) {
        // Multiple ADB devices - let user choose
        printf("\nMultiple ADB devices found:\n");
        PrintDeviceList(state);
        printf("\nPlease select a device using: select <index>\n");
        return 0;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    AppState state;

    // Initialize application
    if (!Initialize(&state)) {
        fprintf(stderr, "Initialization failed. Exiting.\n");
        return 1;
    }

    // Save to global state for cleanup
    memcpy(&g_state, &state, sizeof(AppState));

    // Handle command-line arguments
    if (argc > 1) {
        printf("\nDetected %d file(s) via command line arguments.\n", argc - 1);
        
        // Try to connect to device
        if (!AutoConnect(&state) && state.device_count == 0) {
             printf("\nWaiting for device connection (10s timeout)...\n");
             int retries = 0;
             while (retries < 10) {
                 Sleep(1000);
                 RefreshDeviceList(&state);
                 if (state.device_count > 0) {
                     AutoConnect(&state);
                     break;
                 }
                 retries++;
             }
        }
        
        if (state.device_count == 0) {
             printf("\nError: No ADB device found. Cannot process files.\n");
             printf("Please connect a device and enable USB debugging.\n");
             system("pause");
             return 1;
        }

        // We can process pushes in fastboot mode? No, usually ADB push requires ADB.
        // Fastboot flash is different.
        if (state.current_mode == MODE_FASTBOOT) {
            printf("\nError: Device is in fastboot mode. ADB Push/Install requires ADB mode.\n");
            printf("Please switch to ADB mode.\n");
            system("pause");
            return 1;
        }

        printf("\nStarting batch processing...\n");
        for (int i = 1; i < argc; i++) {
            const char* file_path = argv[i];
            const char* ext = strrchr(file_path, '.');
            
            // Extract filename
            const char* file_name = strrchr(file_path, '\\');
            if (!file_name) file_name = strrchr(file_path, '/');
            if (file_name) file_name++; else file_name = file_path;

            printf("\n----------------------------------------\n");
            printf("Processing: %s\n", file_name);

            int is_apk = (ext && (_stricmp(ext, ".apk") == 0));
            int do_install = 0;

            if (is_apk) {
                printf("File is an APK. Install it? (y/n): ");
                int ch = _getch();
                printf("%c\n", ch); // Echo the char
                if (ch == 'y' || ch == 'Y') {
                    do_install = 1;
                }
            }

            if (do_install) {
                printf("Installing: %s\n", file_path);
                Command cmd = {0};
                strcpy(cmd.name, "install");
                strncpy(cmd.args, file_path, sizeof(cmd.args) - 1);
                CmdInstall(&state, &cmd);
            } else {
                char remote_path[MAX_PATH];
                snprintf(remote_path, sizeof(remote_path), "/storage/emulated/0/%s", file_name);
                printf("Pushing to: %s\n", remote_path);
                PushFile(&state, file_path, remote_path);

                // Check if zip and module
                int is_zip = (ext && (_stricmp(ext, ".zip") == 0));
                if (is_zip) {
                    char seven_zip_path[MAX_PATH];
                    snprintf(seven_zip_path, sizeof(seven_zip_path), "%s\\7za.exe", state.temp_dir);
                    if (IsModuleZip(file_path, seven_zip_path)) {
                        printf("\nDetected Magisk/KSU/APatch Module.\n");
                        // Detect root solution
                        RootSolution sol = DetectRootSolution(&state);
                        if (sol != ROOT_NONE) {
                            InstallRootModule(&state, remote_path, sol);
                        } else {
                            printf("No supported root solution (Magisk/KSU/APatch) detected. Module pushed but not installed.\n");
                        }
                    }
                }
            }
        }
        
        printf("\n----------------------------------------\n");
        printf("Batch processing completed.\n");
        system("pause");
        return 0;
    }

    // Auto-connect to device
    AutoConnect(&state);

    // Start automatic device monitoring (always enabled)
    StartDeviceMonitoring(&state);

    // Run interactive loop
    RunInteractiveLoop(&state);

    return 0;
}
