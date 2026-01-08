#include "utils.h"
#include <time.h>
#include <sys/stat.h>

// Read output from pipe into buffer (helper for RunProcess)
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

// Run generic process and capture output
ProcessResult* RunProcess(const char* executable_path, const char* args[], int arg_count) {
    if (!executable_path || !args || arg_count < 0) {
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
    size_t cmdline_size = strlen(executable_path) + 4; // +4 for quotes and space
    for (int i = 0; i < arg_count; i++) {
        cmdline_size += strlen(args[i]) + 3; // +3 for space and potential quotes
    }

    char* cmdline = (char*)SafeMalloc(cmdline_size);
    snprintf(cmdline, cmdline_size, "\"%s\"", executable_path);
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

// Trim whitespace from both ends of string
char* TrimString(char* str) {
    if (!str) return NULL;

    char* start = str;
    char* end = str + strlen(str) - 1;

    // Trim leading whitespace
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
        start++;
    }

    // Trim trailing whitespace
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }

    // Move trimmed string to beginning
    if (start != str) {
        memmove(str, start, end - start + 1);
    }
    str[end - start + 1] = '\0';

    return str;
}

// Split string by delimiter (modifies original string)
char* SplitString(char* str, char delimiter) {
    if (!str) return NULL;

    char* ptr = strchr(str, delimiter);
    if (ptr) {
        *ptr = '\0';
        return ptr + 1;
    }
    return NULL;
}

// Convert string to lowercase
void StringToLower(char* str) {
    if (!str) return;
    for (; *str; ++str) {
        *str = tolower(*str);
    }
}

// Check if string starts with prefix
int StringStartsWith(const char* str, const char* prefix) {
    if (!str || !prefix) return 0;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

// Join two path components
void JoinPath(char* dest, size_t dest_size, const char* path1, const char* path2) {
    if (!dest || dest_size == 0) return;

    snprintf(dest, dest_size, "%s\\%s", path1, path2);

    // Convert backslashes to forward slashes if needed
    for (char* p = dest; *p; ++p) {
        if (*p == '\\') *p = '/';
    }
}

// Check if file exists
int FileExists(const char* path) {
    if (!path) return 0;
    struct stat statbuf;
    return (stat(path, &statbuf) == 0) && ((statbuf.st_mode & S_IFREG) != 0);
}

// Check if directory exists
int DirectoryExists(const char* path) {
    if (!path) return 0;
    struct stat statbuf;
    return (stat(path, &statbuf) == 0) && ((statbuf.st_mode & S_IFDIR) != 0);
}

// Print error message
void PrintError(AdbErrorCode code, const char* message) {
    fprintf(stderr, "\n[ERROR] ");
    switch (code) {
        case ADB_ERROR_NO_DEVICE:
            fprintf(stderr, "No device connected. Please connect a device and enable USB debugging.\n");
            break;
        case ADB_ERROR_DEVICE_OFFLINE:
            fprintf(stderr, "Device is offline. Please check USB debugging connection.\n");
            break;
        case ADB_ERROR_FILE_NOT_FOUND:
            fprintf(stderr, "File not found: %s\n", message ? message : "unknown");
            break;
        case ADB_ERROR_PERMISSION_DENIED:
            fprintf(stderr, "Permission denied. Check file permissions.\n");
            break;
        case ADB_ERROR_CONNECTION_FAILED:
            fprintf(stderr, "Connection failed. Please check ADB connection.\n");
            break;
        case ADB_ERROR_RESOURCE_EXTRACTION:
            fprintf(stderr, "Failed to extract embedded resources.\n");
            break;
        case ADB_ERROR_INVALID_COMMAND:
            fprintf(stderr, "Invalid command: %s\n", message ? message : "unknown");
            break;
        case ADB_ERROR_TIMEOUT:
            fprintf(stderr, "Operation timed out.\n");
            break;
        default:
            fprintf(stderr, "%s\n", message ? message : "Unknown error");
            break;
    }
}

// Print warning message
void PrintWarning(const char* message) {
    if (message) {
        fprintf(stderr, "[WARNING] %s\n", message);
    }
}

// Print info message
void PrintInfo(const char* message) {
    if (message) {
        printf("[INFO] %s\n", message);
    }
}

// Safe malloc with error checking
void* SafeMalloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        PrintError(ADB_ERROR_UNKNOWN, "Memory allocation failed");
        exit(1);
    }
    return ptr;
}

// Safe calloc with error checking
void* SafeCalloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (!ptr) {
        PrintError(ADB_ERROR_UNKNOWN, "Memory allocation failed");
        exit(1);
    }
    return ptr;
}

// Safe realloc with error checking
void* SafeRealloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        PrintError(ADB_ERROR_UNKNOWN, "Memory reallocation failed");
        exit(1);
    }
    return new_ptr;
}

// Get current timestamp
void GetCurrentTimestamp(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return;

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}
