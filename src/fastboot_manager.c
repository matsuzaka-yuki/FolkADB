#include "fastboot_manager.h"
#include "fastboot_wrapper.h"
#include "adb_wrapper.h"
#include "device_manager.h"
#include "utils.h"
#include <stdio.h>
#include <conio.h>

// Note: GetSelectedFastbootDevice is declared in device_manager.h
// We use it directly from there

// Flash image to partition
int FlashImage(AppState* state, const char* partition, const char* image_path) {
    if (!state || !partition || !image_path) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Invalid arguments");
        return 0;
    }

    // Check if device is selected
    AdbDevice* device = GetSelectedFastbootDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, "No fastboot device selected. Use 'fb_select' first.");
        return 0;
    }

    // Check if image file exists
    if (!FileExists(image_path)) {
        PrintError(ADB_ERROR_IMAGE_NOT_FOUND, image_path);
        return 0;
    }

    // Warning and confirmation
    printf("\n");
    printf("========================================\n");
    printf("     FLASHING PARTITION WARNING\n");
    printf("========================================\n");
    printf("Partition: %s\n", partition);
    printf("Image: %s\n", image_path);
    printf("Device: %s\n", device->serial_id);
    printf("\n");
    printf("WARNING: This will replace the current data on partition '%s'!\n", partition);
    printf("Make sure you have a backup before proceeding.\n");
    printf("\n");
    printf("Press 'y' to confirm, any other key to cancel: ");

    char confirm = _getch();
    printf("%c\n", confirm);

    if (confirm != 'y' && confirm != 'Y') {
        printf("Operation cancelled.\n");
        return 0;
    }

    printf("\nFlashing %s partition...\n", partition);

    ProcessResult* result = FastbootFlash(state->fastboot_path, device->serial_id,
                                          partition, image_path);
    if (!result) {
        PrintError(ADB_ERROR_FLASH_FAILED, "Failed to flash partition");
        return 0;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    if (success) {
        printf("\nPartition flashed successfully.\n");
    } else {
        PrintError(ADB_ERROR_FLASH_FAILED, "Failed to flash partition");
    }

    FreeProcessResult(result);
    return success;
}

// Erase partition
int ErasePartition(AppState* state, const char* partition) {
    if (!state || !partition) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Invalid arguments");
        return 0;
    }

    AdbDevice* device = GetSelectedFastbootDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, "No fastboot device selected");
        return 0;
    }

    printf("\n");
    printf("========================================\n");
    printf("     ERASE PARTITION WARNING\n");
    printf("========================================\n");
    printf("Partition: %s\n", partition);
    printf("Device: %s\n", device->serial_id);
    printf("\n");
    printf("WARNING: This will PERMANENTLY erase all data on partition '%s'!\n", partition);
    printf("This operation cannot be undone.\n");
    printf("\n");
    printf("Press 'y' to confirm, any other key to cancel: ");

    char confirm = _getch();
    printf("%c\n", confirm);

    if (confirm != 'y' && confirm != 'Y') {
        printf("Operation cancelled.\n");
        return 0;
    }

    printf("\nErasing %s partition...\n", partition);

    ProcessResult* result = FastbootErase(state->fastboot_path, device->serial_id, partition);
    if (!result) {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to erase partition");
        return 0;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    if (success) {
        printf("\nPartition erased successfully.\n");
    } else {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to erase partition");
    }

    FreeProcessResult(result);
    return success;
}

// Format partition
int FormatPartition(AppState* state, const char* partition, const char* fs_type) {
    if (!state || !partition || !fs_type) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Invalid arguments");
        return 0;
    }

    AdbDevice* device = GetSelectedFastbootDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, "No fastboot device selected");
        return 0;
    }

    printf("\nFormatting partition %s with filesystem %s...\n", partition, fs_type);

    ProcessResult* result = FastbootFormat(state->fastboot_path, device->serial_id,
                                           partition, fs_type);
    if (!result) {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to format partition");
        return 0;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    if (success) {
        printf("Partition formatted successfully.\n");
    } else {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to format partition");
    }

    FreeProcessResult(result);
    return success;
}

// Unlock bootloader
int UnlockBootloader(AppState* state) {
    if (!state) return 0;

    AdbDevice* device = GetSelectedFastbootDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, "No fastboot device selected");
        return 0;
    }

    printf("\n");
    printf("========================================\n");
    printf("    BOOTLOADER UNLOCK WARNING\n");
    printf("========================================\n");
    printf("Device: %s\n", device->serial_id);
    printf("\n");
    printf("DANGER: This will UNLOCK your bootloader!\n");
    printf("\n");
    printf("This will:\n");
    printf("  - Void your warranty\n");
    printf("  - Wipe all data on your device\n");
    printf("  - Allow custom ROMs and recoveries\n");
    printf("  - Make your device less secure\n");
    printf("\n");
    printf("Please make sure you understand the risks.\n");
    printf("\n");
    printf("Press 'y' to confirm unlock, any other key to cancel: ");

    char confirm = _getch();
    printf("%c\n", confirm);

    if (confirm != 'y' && confirm != 'Y') {
        printf("Operation cancelled.\n");
        return 0;
    }

    printf("\nUnlocking bootloader...\n");
    printf("Follow the instructions on your device screen.\n");

    ProcessResult* result = FastbootUnlock(state->fastboot_path, device->serial_id);
    if (!result) {
        PrintError(ADB_ERROR_UNLOCK_FAILED, "Failed to unlock bootloader");
        return 0;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    if (success) {
        printf("\nBootloader unlock command sent.\n");
        printf("Check your device screen for confirmation.\n");
    } else {
        PrintError(ADB_ERROR_UNLOCK_FAILED, "Failed to unlock bootloader");
    }

    FreeProcessResult(result);
    return success;
}

// Lock bootloader
int LockBootloader(AppState* state) {
    if (!state) return 0;

    AdbDevice* device = GetSelectedFastbootDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, "No fastboot device selected");
        return 0;
    }

    printf("\n");
    printf("========================================\n");
    printf("    BOOTLOADER LOCK WARNING\n");
    printf("========================================\n");
    printf("Device: %s\n", device->serial_id);
    printf("\n");
    printf("WARNING: This will RELOCK your bootloader!\n");
    printf("\n");
    printf("This will:\n");
    printf("  - Prevent custom ROMs and recoveries\n");
    printf("  - May require wiping data to unlock again\n");
    printf("  - Restore some security features\n");
    printf("\n");
    printf("Press 'y' to confirm lock, any other key to cancel: ");

    char confirm = _getch();
    printf("%c\n", confirm);

    if (confirm != 'y' && confirm != 'Y') {
        printf("Operation cancelled.\n");
        return 0;
    }

    printf("\nLocking bootloader...\n");

    ProcessResult* result = FastbootLock(state->fastboot_path, device->serial_id);
    if (!result) {
        PrintError(ADB_ERROR_UNKNOWN, "Failed to lock bootloader");
        return 0;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    if (success) {
        printf("\nBootloader lock command sent.\n");
    } else {
        PrintError(ADB_ERROR_UNKNOWN, "Failed to lock bootloader");
    }

    FreeProcessResult(result);
    return success;
}

// Execute OEM command
int ExecuteOemCommand(AppState* state, const char* oem_cmd) {
    if (!state || !oem_cmd) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Invalid arguments");
        return 0;
    }

    AdbDevice* device = GetSelectedFastbootDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, "No fastboot device selected");
        return 0;
    }

    printf("Executing OEM command: %s\n", oem_cmd);

    ProcessResult* result = FastbootOemCommand(state->fastboot_path, device->serial_id, oem_cmd);
    if (!result) {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to execute OEM command");
        return 0;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    FreeProcessResult(result);
    return success;
}

// Reboot fastboot device
int RebootFastbootDevice(AppState* state, const char* mode) {
    if (!state) return 0;

    AdbDevice* device = GetSelectedFastbootDevice(state);
    if (!device) {
        // Try to refresh device list and auto-select
        RefreshFastbootDeviceList(state);

        if (state->fastboot_device_count > 0) {
            SelectFastbootDevice(state, 0);
            device = GetSelectedFastbootDevice(state);
        }

        if (!device) {
            PrintError(ADB_ERROR_NO_DEVICE, "No fastboot device selected");
            return 0;
        }
    }

    printf("Rebooting device");
    if (mode) {
        printf(" to %s mode", mode);
    }
    printf("...\n");

    ProcessResult* result = FastbootReboot(state->fastboot_path, device->serial_id, mode);
    if (!result) {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to reboot device");
        return 0;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    if (success) {
        printf("Device is rebooting...\n");
    } else {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to reboot device");
    }

    FreeProcessResult(result);
    return success;
}

// Get fastboot variable
int GetFastbootVar(AppState* state, const char* var_name) {
    if (!state) return 0;

    AdbDevice* device = GetSelectedFastbootDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, "No fastboot device selected");
        return 0;
    }

    if (var_name) {
        printf("Getting variable: %s\n", var_name);
        ProcessResult* result = FastbootGetVar(state->fastboot_path, device->serial_id, var_name);

        if (result && result->stdout_data) {
            printf("%s\n", result->stdout_data);
        }

        if (result) {
            FreeProcessResult(result);
            return (result->exit_code == 0);
        }
        return 0;
    } else {
        // Get all variables
        printf("Getting all variables...\n");
        ProcessResult* result = FastbootGetAllVars(state->fastboot_path, device->serial_id);

        if (result && result->stdout_data) {
            printf("%s\n", result->stdout_data);
        }

        if (result) {
            FreeProcessResult(result);
            return (result->exit_code == 0);
        }
        return 0;
    }
}

// Activate fastboot slot
int ActivateFastbootSlot(AppState* state, const char* slot) {
    if (!state || !slot) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Invalid arguments");
        return 0;
    }

    // Validate slot name
    if (strcmp(slot, "a") != 0 && strcmp(slot, "A") != 0 &&
        strcmp(slot, "b") != 0 && strcmp(slot, "B") != 0) {
        printf("Error: Invalid slot '%s'. Must be 'a' or 'b'.\n", slot);
        return 0;
    }

    AdbDevice* device = GetSelectedFastbootDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, "No fastboot device selected");
        return 0;
    }

    printf("Activating slot %s...\n", slot);

    ProcessResult* result = FastbootActivateSlot(state->fastboot_path, device->serial_id, slot);
    if (!result) {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to activate slot");
        return 0;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    if (success) {
        printf("Slot %s activated successfully.\n", slot);
    } else {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to activate slot");
    }

    FreeProcessResult(result);
    return success;
}

// Wipe fastboot partition
int WipeFastbootPartition(AppState* state, const char* partition) {
    if (!state || !partition) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Invalid arguments");
        return 0;
    }

    AdbDevice* device = GetSelectedFastbootDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, "No fastboot device selected");
        return 0;
    }

    printf("\n");
    printf("========================================\n");
    printf("       WIPE DATA WARNING\n");
    printf("========================================\n");
    printf("Partition: %s\n", partition);
    printf("Device: %s\n", device->serial_id);
    printf("\n");
    printf("WARNING: This will WIPE ALL DATA on partition '%s'!\n", partition);
    printf("This will perform a factory reset and cannot be undone.\n");
    printf("\n");
    printf("Press 'y' to confirm, any other key to cancel: ");

    char confirm = _getch();
    printf("%c\n", confirm);

    if (confirm != 'y' && confirm != 'Y') {
        printf("Operation cancelled.\n");
        return 0;
    }

    printf("\nWiping partition %s...\n", partition);

    ProcessResult* result = FastbootWipe(state->fastboot_path, device->serial_id, partition);
    if (!result) {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to wipe partition");
        return 0;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    if (success) {
        printf("\nPartition wiped successfully.\n");
    } else {
        PrintError(ADB_ERROR_FASTBOOT_FAILED, "Failed to wipe partition");
    }

    FreeProcessResult(result);
    return success;
}

// Show fastboot device info
int ShowFastbootDeviceInfo(AppState* state) {
    if (!state) return 0;

    AdbDevice* device = GetSelectedFastbootDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, "No fastboot device selected");
        return 0;
    }

    printf("\n");
    printf("========================================\n");
    printf("      Fastboot Device Information\n");
    printf("========================================\n");
    printf("Serial: %s\n", device->serial_id);
    printf("Status: %s\n", device->status);

    // Get additional info
    printf("\nGetting device variables...\n");

    ProcessResult* result = FastbootGetAllVars(state->fastboot_path, device->serial_id);
    if (result && result->stdout_data) {
        printf("%s\n", result->stdout_data);
    }

    if (result) FreeProcessResult(result);

    printf("========================================\n");

    return 1;
}
