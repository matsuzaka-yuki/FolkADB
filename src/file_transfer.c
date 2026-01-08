#include "file_transfer.h"
#include "adb_wrapper.h"
#include "utils.h"
#include <stdio.h>

// Push file to device
int PushFile(AppState* state, const char* local_path, const char* remote_path) {
    if (!state || !local_path || !remote_path) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Invalid arguments");
        return 0;
    }

    // Check if local file exists
    if (!FileExists(local_path)) {
        PrintError(ADB_ERROR_FILE_NOT_FOUND, local_path);
        return 0;
    }

    // Check if device is selected
    AdbDevice* device = GetSelectedDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, NULL);
        return 0;
    }

    printf("Pushing %s to %s:%s...\n", local_path, device->serial_id, remote_path);

    ProcessResult* result = AdbPushFile(state->adb_path, device->serial_id, local_path, remote_path);
    if (!result) {
        PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to push file");
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
        printf("File pushed successfully.\n");
    } else {
        PrintError(ADB_ERROR_UNKNOWN, "Failed to push file");
    }

    FreeProcessResult(result);
    return success;
}

// Pull file from device
int PullFile(AppState* state, const char* remote_path, const char* local_path) {
    if (!state || !remote_path) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Invalid arguments");
        return 0;
    }

    // Check if device is selected
    AdbDevice* device = GetSelectedDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, NULL);
        return 0;
    }

    // Use filename from remote path if local path not specified
    char local_file[MAX_PATH];
    if (!local_path || strlen(local_path) == 0) {
        // Extract filename from remote path
        const char* filename = strrchr(remote_path, '/');
        if (filename) {
            filename++;
        } else {
            filename = remote_path;
        }
        snprintf(local_file, sizeof(local_file), ".\\%s", filename);
    } else {
        snprintf(local_file, sizeof(local_file), "%s", local_path);
    }

    printf("Pulling %s:%s to %s...\n", device->serial_id, remote_path, local_file);

    ProcessResult* result = AdbPullFile(state->adb_path, device->serial_id, remote_path, local_file);
    if (!result) {
        PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to pull file");
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
        printf("File pulled successfully.\n");
    } else {
        PrintError(ADB_ERROR_UNKNOWN, "Failed to pull file");
    }

    FreeProcessResult(result);
    return success;
}

// List files on device
int ListRemoteFiles(AppState* state, const char* remote_path) {
    if (!state) return 0;

    // Check if device is selected
    AdbDevice* device = GetSelectedDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, NULL);
        return 0;
    }

    const char* path = remote_path ? remote_path : "/sdcard";

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "ls -la \"%s\"", path);

    printf("Listing %s:%s\n", device->serial_id, path);
    printf("----------------------------------------\n");

    ProcessResult* result = AdbShellCommand(state->adb_path, device->serial_id, cmd);
    if (!result) {
        PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to list files");
        return 0;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    printf("----------------------------------------\n");

    FreeProcessResult(result);
    return success;
}

// Delete file on device
int DeleteRemoteFile(AppState* state, const char* remote_path) {
    if (!state || !remote_path) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Invalid arguments");
        return 0;
    }

    // Check if device is selected
    AdbDevice* device = GetSelectedDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, NULL);
        return 0;
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm \"%s\"", remote_path);

    printf("Deleting %s:%s...\n", device->serial_id, remote_path);

    ProcessResult* result = AdbShellCommand(state->adb_path, device->serial_id, cmd);
    if (!result) {
        PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to delete file");
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
        printf("File deleted successfully.\n");
    } else {
        PrintError(ADB_ERROR_UNKNOWN, "Failed to delete file");
    }

    FreeProcessResult(result);
    return success;
}

// Create directory on device
int CreateRemoteDirectory(AppState* state, const char* remote_path) {
    if (!state || !remote_path) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Invalid arguments");
        return 0;
    }

    // Check if device is selected
    AdbDevice* device = GetSelectedDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, NULL);
        return 0;
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", remote_path);

    printf("Creating directory %s:%s...\n", device->serial_id, remote_path);

    ProcessResult* result = AdbShellCommand(state->adb_path, device->serial_id, cmd);
    if (!result) {
        PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to create directory");
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
        printf("Directory created successfully.\n");
    } else {
        PrintError(ADB_ERROR_UNKNOWN, "Failed to create directory");
    }

    FreeProcessResult(result);
    return success;
}
