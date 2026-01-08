#include "fastboot_wrapper.h"
#include "utils.h"
#include <stdarg.h>

// Read output from pipe into buffer (same as adb_wrapper)
static void ReadOutputToBuffer(HANDLE hRead, char** data, size_t* size) {
    DWORD buffer_size = 4096;
    *data = (char*)SafeMalloc(buffer_size);
    *size = 0;

    DWORD bytes_read;
    char temp[4096];
    while (ReadFile(hRead, temp, sizeof(temp), &bytes_read, NULL) && bytes_read > 0) {
        if (*size + bytes_read >= buffer_size) {
            buffer_size *= 2;
            *data = (char*)SafeRealloc(*data, buffer_size);
        }
        memcpy(*data + *size, temp, bytes_read);
        *size += bytes_read;
    }

    // Null-terminate
    if (*size + 1 >= buffer_size) {
        *data = (char*)SafeRealloc(*data, buffer_size + 1);
    }
    (*data)[*size] = '\0';
}

// Spawn fastboot.exe and capture output
ProcessResult* RunFastbootCommand(const char* fastboot_path, const char* args[], int arg_count) {
    if (!fastboot_path || !args || arg_count <= 0) {
        return NULL;
    }

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE stdout_read, stdout_write;
    HANDLE stderr_read, stderr_write;

    // Create pipes for stdout and stderr
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
        !CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        return NULL;
    }

    // Set read handles to not be inherited
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    // Build command line
    size_t cmdline_size = strlen(fastboot_path) + 4;
    for (int i = 0; i < arg_count; i++) {
        cmdline_size += strlen(args[i]) + 3;
    }

    char* cmdline = (char*)SafeMalloc(cmdline_size);
    snprintf(cmdline, cmdline_size, "\"%s\"", fastboot_path);
    for (int i = 0; i < arg_count; i++) {
        strcat(cmdline, " ");
        if (strchr(args[i], ' ')) {
            strcat(cmdline, "\"");
            strcat(cmdline, args[i]);
            strcat(cmdline, "\"");
        } else {
            strcat(cmdline, args[i]);
        }
    }

    // Setup startup info
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {0};

    // Create process
    BOOL success = CreateProcessA(
        NULL,
        cmdline,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    // Close write ends
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!success) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        free(cmdline);
        return NULL;
    }

    // Allocate result structure
    ProcessResult* result = (ProcessResult*)SafeCalloc(1, sizeof(ProcessResult));

    // Read output
    ReadOutputToBuffer(stdout_read, &result->stdout_data, &result->stdout_size);
    ReadOutputToBuffer(stderr_read, &result->stderr_data, &result->stderr_size);

    // Wait for process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    result->exit_code = (int)exit_code;

    // Cleanup
    CloseHandle(stdout_read);
    CloseHandle(stderr_read);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    free(cmdline);

    return result;
}

// List fastboot devices
ProcessResult* FastbootDevices(const char* fastboot_path) {
    const char* args[] = { "devices" };
    return RunFastbootCommand(fastboot_path, args, 1);
}

// Get device variable
ProcessResult* FastbootGetVar(const char* fastboot_path, const char* device_serial, const char* var_name) {
    if (!var_name) return NULL;

    int idx = 0;
    const char* args[4];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "getvar";
    args[idx++] = var_name;

    return RunFastbootCommand(fastboot_path, args, idx);
}

// Get all variables
ProcessResult* FastbootGetAllVars(const char* fastboot_path, const char* device_serial) {
    int idx = 0;
    const char* args[3];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "getvar";
    args[idx++] = "all";

    return RunFastbootCommand(fastboot_path, args, idx);
}

// Flash partition with image
ProcessResult* FastbootFlash(const char* fastboot_path, const char* device_serial,
                             const char* partition, const char* image_path) {
    if (!partition || !image_path) return NULL;

    int idx = 0;
    const char* args[5];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "flash";
    args[idx++] = partition;
    args[idx++] = image_path;

    return RunFastbootCommand(fastboot_path, args, idx);
}

// Erase partition
ProcessResult* FastbootErase(const char* fastboot_path, const char* device_serial,
                             const char* partition) {
    if (!partition) return NULL;

    int idx = 0;
    const char* args[4];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "erase";
    args[idx++] = partition;

    return RunFastbootCommand(fastboot_path, args, idx);
}

// Format partition
ProcessResult* FastbootFormat(const char* fastboot_path, const char* device_serial,
                              const char* partition, const char* fs_type) {
    if (!partition || !fs_type) return NULL;

    int idx = 0;
    const char* args[5];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "format";
    args[idx++] = partition;
    args[idx++] = fs_type;

    return RunFastbootCommand(fastboot_path, args, idx);
}

// Unlock bootloader
ProcessResult* FastbootUnlock(const char* fastboot_path, const char* device_serial) {
    int idx = 0;
    const char* args[3];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "flashing";
    args[idx++] = "unlock";

    return RunFastbootCommand(fastboot_path, args, idx);
}

// Lock bootloader
ProcessResult* FastbootLock(const char* fastboot_path, const char* device_serial) {
    int idx = 0;
    const char* args[3];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "flashing";
    args[idx++] = "lock";

    return RunFastbootCommand(fastboot_path, args, idx);
}

// Execute OEM command
ProcessResult* FastbootOemCommand(const char* fastboot_path, const char* device_serial,
                                  const char* oem_cmd) {
    if (!oem_cmd) return NULL;

    int idx = 0;
    const char* args[4];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "oem";
    args[idx++] = oem_cmd;

    return RunFastbootCommand(fastboot_path, args, idx);
}

// Reboot device
ProcessResult* FastbootReboot(const char* fastboot_path, const char* device_serial,
                              const char* mode) {
    int idx = 0;
    const char* args[3];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "reboot";

    if (mode && strcmp(mode, "system") != 0) {
        // Add mode as additional argument (reboot recovery, reboot bootloader)
        // Note: fastboot uses "reboot-bootloader" not "reboot bootloader"
        char reboot_cmd[64];
        snprintf(reboot_cmd, sizeof(reboot_cmd), "reboot-%s", mode);
        args[idx - 1] = reboot_cmd;
    }

    return RunFastbootCommand(fastboot_path, args, idx);
}

// Reboot to bootloader
ProcessResult* FastbootRebootBootloader(const char* fastboot_path, const char* device_serial) {
    return FastbootReboot(fastboot_path, device_serial, "bootloader");
}

// Activate slot (A/B partition scheme)
ProcessResult* FastbootActivateSlot(const char* fastboot_path, const char* device_serial,
                                    const char* slot) {
    if (!slot) return NULL;

    int idx = 0;
    const char* args[4];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "set_active";
    args[idx++] = slot;

    return RunFastbootCommand(fastboot_path, args, idx);
}

// Wipe partition
ProcessResult* FastbootWipe(const char* fastboot_path, const char* device_serial,
                            const char* partition) {
    if (!partition) return NULL;

    int idx = 0;
    const char* args[4];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "wipe";
    args[idx++] = partition;

    return RunFastbootCommand(fastboot_path, args, idx);
}

// Parse fastboot device list
int ParseFastbootDeviceList(const char* output, AdbDevice* devices, int max_devices) {
    if (!output || !devices || max_devices <= 0) return 0;

    int count = 0;
    char* copy = _strdup(output);
    if (!copy) return 0;

    char* line = strtok(copy, "\n");
    while (line != NULL && count < max_devices) {
        TrimString(line);
        if (strlen(line) == 0) {
            line = strtok(NULL, "\n");
            continue;
        }

        // Parse line: "serial\tfastboot"
        char serial[256] = {0};
        char status[64] = {0};

        if (sscanf(line, "%s %s", serial, status) == 2) {
            strncpy(devices[count].serial_id, serial, sizeof(devices[count].serial_id) - 1);
            strncpy(devices[count].status, status, sizeof(devices[count].status) - 1);
            count++;
        }

        line = strtok(NULL, "\n");
    }

    free(copy);
    return count;
}

// Extract fastboot variable value
char* ExtractFastbootVar(const char* output, const char* var_name) {
    if (!output || !var_name) return NULL;

    // Format: "var_name: value" or "(bootloader) var_name: value"
    char search[512];
    snprintf(search, sizeof(search), "%s:", var_name);

    char* found = strstr(output, search);
    if (!found) return NULL;

    found += strlen(search);
    while (*found == ' ' || *found == '\t') found++;

    char* end = strchr(found, '\n');
    if (end) {
        size_t len = end - found;
        char* value = (char*)SafeMalloc(len + 1);
        memcpy(value, found, len);
        value[len] = '\0';
        TrimString(value);
        return value;
    }

    return _strdup(found);
}
