#include "module_installer.h"
#include "adb_wrapper.h"
#include "device_manager.h"
#include "utils.h"

// Check if zip file is a module (contains module.prop)
int IsModuleZip(const char* zip_path) {
    if (!zip_path) return 0;

    // Locate 7za.exe
    char seven_zip_path[MAX_PATH];
    if (FileExists("bin\\7za.exe")) {
        strcpy(seven_zip_path, "bin\\7za.exe");
    } else if (FileExists("..\\bin\\7za.exe")) {
        strcpy(seven_zip_path, "..\\bin\\7za.exe");
    } else {
        // Try to assume it's in the same dir as current executable if bin not found
        // But for now, if not found, we can't check.
        return 0; 
    }

    const char* args[] = { "l", zip_path };
    ProcessResult* res = RunProcess(seven_zip_path, args, 2);
    if (!res) return 0;

    int found = 0;
    if (res->stdout_data) {
        if (strstr(res->stdout_data, "module.prop")) {
            found = 1;
        }
    }

    FreeProcessResult(res);
    return found;
}

// Detect root solution
RootSolution DetectRootSolution(AppState* state) {
    if (!state) return ROOT_NONE;

    AdbDevice* dev = GetSelectedDevice(state);
    if (!dev) return ROOT_NONE;

    // Check APatch
    // Use su -c to ensure we are checking as root/root context if needed, 
    // though typically version checks might work as shell. 
    // User requested su usage.
    ProcessResult* res = AdbShellCommand(state->adb_path, dev->serial_id, "su -c \"apd -V\"");
    if (res && res->exit_code == 0) {
        FreeProcessResult(res);
        return ROOT_APATCH;
    }
    FreeProcessResult(res);

    // Check KernelSU
    res = AdbShellCommand(state->adb_path, dev->serial_id, "su -c \"ksud -V\"");
    if (res && res->exit_code == 0) {
        FreeProcessResult(res);
        return ROOT_KSU;
    }
    FreeProcessResult(res);

    // Check Magisk
    res = AdbShellCommand(state->adb_path, dev->serial_id, "su -c \"magisk -V\"");
    if (res && res->exit_code == 0) {
        FreeProcessResult(res);
        return ROOT_MAGISK;
    }
    FreeProcessResult(res);

    return ROOT_NONE;
}

// Install module
void InstallRootModule(AppState* state, const char* remote_zip_path, RootSolution solution) {
    if (!state || !remote_zip_path) return;
    
    AdbDevice* dev = GetSelectedDevice(state);
    if (!dev) return;

    char cmd[MAX_PATH + 64];
    ProcessResult* res = NULL;

    switch (solution) {
        case ROOT_APATCH:
            printf("Detected Root Solution: APatch/FolkPatch\n");
            // apd module install <zip>
            snprintf(cmd, sizeof(cmd), "su -c \"/data/adb/apd module install \\\"%s\\\"\"", remote_zip_path);
            break;
        case ROOT_KSU:
            printf("Detected Root Solution: KernelSU\n");
            // ksud module install <zip>
            snprintf(cmd, sizeof(cmd), "su -c \"/data/adb/ksud module install \\\"%s\\\"\"", remote_zip_path);
            break;
        case ROOT_MAGISK:
            printf("Detected Root Solution: Magisk\n");
            // magisk --install-module <zip>
            snprintf(cmd, sizeof(cmd), "su -c \"magisk --install-module \\\"%s\\\"\"", remote_zip_path);
            break;
        default:
            printf("No supported root solution detected.\n");
            return;
    }

    printf("Installing module...\n");
    res = AdbShellCommand(state->adb_path, dev->serial_id, cmd);
    if (res) {
        if (res->stdout_data) printf("%s", res->stdout_data);
        if (res->stderr_data) printf("%s", res->stderr_data);
        
        FreeProcessResult(res);
    } else {
        printf("Failed to execute install command.\n");
    }
}

// Download file from URL to destination using system curl or PowerShell
int DownloadFile(const char* url, const char* dest_path) {
    if (!url || !dest_path) return 0;

    printf("Downloading: %s\n", url);
    printf("To: %s\n", dest_path);

    char cmd[2048];
    // Try using curl first (usually available on Windows 10+)
    // -L: Follow redirects
    // -o: Write to file
    // We use system() to let curl show its progress bar directly in the console
    snprintf(cmd, sizeof(cmd), "curl -L \"%s\" -o \"%s\"", url, dest_path);
    
    int ret = system(cmd);

    if (ret != 0) {
        // Fallback to PowerShell if curl is missing or fails
        printf("Curl download failed or not found. Falling back to PowerShell...\n");
        
        // Use PowerShell Invoke-WebRequest
        // Note: Progress bar might look different or be hidden depending on console mode,
        // but system() gives it the best chance to show up compared to pipes.
        snprintf(cmd, sizeof(cmd), "powershell -Command \"Invoke-WebRequest -Uri '%s' -OutFile '%s'\"", url, dest_path);
        ret = system(cmd);
    }

    if (ret == 0 && FileExists(dest_path)) {
        return 1;
    }

    printf("Download failed.\n");
    return 0;
}
