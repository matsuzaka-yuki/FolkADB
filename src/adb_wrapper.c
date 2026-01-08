#include "adb_wrapper.h"
#include "utils.h"
#include <stdarg.h>

// Read output from pipe into buffer
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

// Spawn adb.exe and capture output
ProcessResult* RunAdbCommand(const char* adb_path, const char* args[], int arg_count) {
    if (!adb_path || !args || arg_count <= 0) {
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
    size_t cmdline_size = strlen(adb_path) + 4; // +4 for quotes and space
    for (int i = 0; i < arg_count; i++) {
        cmdline_size += strlen(args[i]) + 3; // +3 for space and potential quotes
    }

    char* cmdline = (char*)SafeMalloc(cmdline_size);
    snprintf(cmdline, cmdline_size, "\"%s\"", adb_path);
    for (int i = 0; i < arg_count; i++) {
        strcat(cmdline, " ");
        // Quote arguments that contain spaces
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

    // Close write ends (we only read)
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

// Free process result
void FreeProcessResult(ProcessResult* result) {
    if (!result) return;

    SAFE_FREE(result->stdout_data);
    SAFE_FREE(result->stderr_data);
    SAFE_FREE(result);
}

// List connected devices
ProcessResult* AdbDevices(const char* adb_path) {
    const char* args[] = { "devices", "-l" };
    return RunAdbCommand(adb_path, args, 2);
}

// Execute shell command
ProcessResult* AdbShellCommand(const char* adb_path, const char* device_serial, const char* command) {
    if (!command) return NULL;

    int arg_count = 2; // "shell" + command
    int serial_index = 0;

    const char* args[4];
    int idx = 0;

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
        serial_index = 1;
    }

    args[idx++] = "shell";
    args[idx++] = command;

    return RunAdbCommand(adb_path, args, idx);
}

// Get device property
ProcessResult* AdbGetProp(const char* adb_path, const char* device_serial, const char* prop) {
    if (!prop) return NULL;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "getprop %s", prop);

    return AdbShellCommand(adb_path, device_serial, cmd);
}

// Push file to device
ProcessResult* AdbPushFile(const char* adb_path, const char* device_serial,
                           const char* local_path, const char* remote_path) {
    if (!local_path || !remote_path) return NULL;

    int idx = 0;
    const char* args[6];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "push";
    args[idx++] = local_path;
    args[idx++] = remote_path;

    return RunAdbCommand(adb_path, args, idx);
}

// Pull file from device
ProcessResult* AdbPullFile(const char* adb_path, const char* device_serial,
                           const char* remote_path, const char* local_path) {
    if (!remote_path) return NULL;

    int idx = 0;
    const char* args[6];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "pull";
    args[idx++] = remote_path;

    if (local_path) {
        args[idx++] = local_path;
    }

    return RunAdbCommand(adb_path, args, idx);
}

// Install APK
ProcessResult* AdbInstallApk(const char* adb_path, const char* device_serial, const char* apk_path) {
    if (!apk_path) return NULL;

    int idx = 0;
    const char* args[5];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "install";
    args[idx++] = apk_path;

    return RunAdbCommand(adb_path, args, idx);
}

// Uninstall package
ProcessResult* AdbUninstallPackage(const char* adb_path, const char* device_serial, const char* package) {
    if (!package) return NULL;

    int idx = 0;
    const char* args[5];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "uninstall";
    args[idx++] = package;

    return RunAdbCommand(adb_path, args, idx);
}

// Reboot device
ProcessResult* AdbReboot(const char* adb_path, const char* device_serial, const char* mode) {
    int idx = 0;
    const char* args[5];

    if (device_serial) {
        args[idx++] = "-s";
        args[idx++] = device_serial;
    }

    args[idx++] = "reboot";

    if (mode && strcmp(mode, "system") != 0) {
        args[idx++] = mode;
    }

    return RunAdbCommand(adb_path, args, idx);
}

// Parse device list from adb devices output
int ParseDeviceList(const char* output, AdbDevice* devices, int max_devices) {
    if (!output || !devices || max_devices <= 0) return 0;

    int count = 0;
    char* copy = _strdup(output);
    if (!copy) return 0;

    char* line = strtok(copy, "\n");
    while (line != NULL && count < max_devices) {
        // Skip header
        if (strstr(line, "List of devices") != NULL ||
            strstr(line, "daemon") != NULL) {
            line = strtok(NULL, "\n");
            continue;
        }

        TrimString(line);
        if (strlen(line) == 0) {
            line = strtok(NULL, "\n");
            continue;
        }

        // Parse line: "serial device product:model ..."
        char serial[256] = {0};
        char status[64] = {0};

        if (sscanf(line, "%s %s", serial, status) == 2) {
            strncpy(devices[count].serial_id, serial, sizeof(devices[count].serial_id) - 1);
            strncpy(devices[count].status, status, sizeof(devices[count].status) - 1);

            // Parse product info if available
            char* product = strstr(line, "product:");
            if (product) {
                char model[256];
                if (sscanf(product, "product:%s", model) == 1) {
                    // Remove trailing comma or space
                    char* end = strchr(model, ',');
                    if (end) *end = '\0';
                    end = strchr(model, ' ');
                    if (end) *end = '\0';
                    strncpy(devices[count].model, model, sizeof(devices[count].model) - 1);
                }
            }

            // Parse device info if available
            char* device = strstr(line, "device:");
            if (device) {
                char device_name[256];
                if (sscanf(device, "device:%s", device_name) == 1) {
                    char* end = strchr(device_name, ',');
                    if (end) *end = '\0';
                    end = strchr(device_name, ' ');
                    if (end) *end = '\0';
                    strncpy(devices[count].device_name, device_name, sizeof(devices[count].device_name) - 1);
                }
            }

            count++;
        }

        line = strtok(NULL, "\n");
    }

    free(copy);
    return count;
}

// Extract property value from getprop output
char* ExtractPropValue(const char* output, const char* prop_name) {
    if (!output || !prop_name) return NULL;

    // Format: [prop.name]: [value]
    char search[512];
    snprintf(search, sizeof(search), "[%s]:", prop_name);

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
        return value;
    }

    return _strdup(found);
}
