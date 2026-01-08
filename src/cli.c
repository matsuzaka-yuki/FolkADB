#include "cli.h"
#include "device_manager.h"
#include "file_transfer.h"
#include "fastboot_manager.h"
#include "adb_wrapper.h"
#include "utils.h"
#include "module_installer.h"
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <sys/stat.h>

// Forward declarations
int CmdDli(AppState* state, const Command* cmd);

// Track if prompt needs to be refreshed
static volatile int g_prompt_needs_refresh = 0;

// Command history
#define MAX_HISTORY 50
static char g_history[MAX_HISTORY][4096];
static int g_history_count = 0;
static int g_history_index = -1;
static int g_current_history_view = -1;

// Add command to history
static void AddToHistory(const char* cmd) {
    if (!cmd || strlen(cmd) == 0) return;
    
    // Don't add if same as last one
    if (g_history_count > 0 && strcmp(g_history[(g_history_count - 1) % MAX_HISTORY], cmd) == 0) {
        g_history_index = g_history_count - 1;
        return;
    }
    
    strncpy(g_history[g_history_count % MAX_HISTORY], cmd, 4095);
    g_history_count++;
    g_history_index = g_history_count - 1;
}

// Callback function to refresh prompt from monitoring thread
static void RefreshPromptCallback(void) {
    g_prompt_needs_refresh = 1;
}

// Show banner
void ShowBanner(void) {
    printf("\n");
    printf("========================================\n");
    printf("   ____        __   __     ___    ___    ___ \n");
    printf("  / __/ ___   / /  / /__  / _ |  / _ \\  / _ )\n");
    printf(" / _/  / _ \\ / /  /  '_/ / __ | / // / / _  |\n");
    printf("/_/    \\___//_/  /_/\\_\\ /_/ |_|/____/ /____/ \n");
    printf("\n");
    printf("   Version: %s\n", APP_VERSION);
    printf("   Portable Android Debug Bridge Tool\n");
    printf("   Author: Matsuzaka Yuki\n");
    printf("   Github: https://github.com/matsuzaka-yuki/FolkADB\n");
    printf("========================================\n");
    printf("\n");
}

// Get prompt string without newline
void GetPromptString(const AppState* state, char* buffer, size_t size) {
    if (state->current_mode == MODE_FASTBOOT) {
        AdbDevice* device = GetSelectedFastbootDevice(state);
        if (device) {
            snprintf(buffer, size, "fastboot [%s]> ", device->serial_id);
        } else {
            snprintf(buffer, size, "fastboot [no device]> ");
        }
    } else {
        AdbDevice* device = GetSelectedDevice(state);
        if (device) {
            snprintf(buffer, size, "adb [%s]> ", device->serial_id);
        } else {
            snprintf(buffer, size, "adb [no device]> ");
        }
    }
}

// Display command prompt
void DisplayPrompt(const AppState* state) {
    char buffer[256];
    GetPromptString(state, buffer, sizeof(buffer));
    printf("\n%s", buffer);
    fflush(stdout);
}

// Show help
void ShowHelp(AppState* state) {
    printf("\n");
    printf("========================================\n");
    printf("              Commands\n");
    printf("========================================\n");
    printf("\n");
    
    if (state->current_mode == MODE_ADB) {
        printf("ADB Device Management:\n");
        printf("  devices, dev             List connected devices\n");
        printf("  select <index|serial>    Select device\n");
        printf("  info                     Show device information\n");
        printf("\n");
        printf("ADB File Operations:\n");
        printf("  push <local> <remote>    Push file to device\n");
        printf("  pull <remote> [local]    Pull file from device\n");
        printf("  ls <remote_path>         List files on device\n");
        printf("  rm <remote_path>         Delete file on device\n");
        printf("  mkdir <remote_path>      Create directory on device\n");
        printf("\n");
        printf("ADB Shell & System:\n");
        printf("  shell [command]          Execute shell command or enter interactive mode\n");
        printf("                           - Enter shell: type 'shell'\n");
        printf("                           - Get root: type 'su' inside shell\n");
        printf("                           - Exit shell: type 'exit' inside shell\n");
        printf("  sudo <command>           Execute command with root privileges (su -c)\n");
        printf("  install <apk>            Install APK file\n");
        printf("  uninstall <package>      Uninstall package\n");
        printf("  reboot [mode]            Reboot device (system/recovery/bootloader)\n");
        printf("  dli <url>                Download, push and install module from URL\n");
    } else {
        printf("Fastboot Commands:\n");
        printf("  devices           List fastboot devices\n");
        printf("  select <id>       Select fastboot device\n");
        printf("  info              Show fastboot device info\n");
        printf("  flash <part> <img> Flash partition with image\n");
        printf("  erase <part>      Erase partition\n");
        printf("  format <part> <fs> Format partition\n");
        printf("  reboot [mode]     Reboot device\n");
        printf("  getvar <var>      Get variable\n");
        printf("  oem <cmd>         Execute OEM command\n");
        printf("  unlock            Unlock bootloader\n");
        printf("  lock              Lock bootloader\n");
        printf("  wipe <part>       Wipe data\n");
        printf("  activate <slot>   Activate slot\n");
        printf("\n");
        printf("To use ADB commands, switch back to ADB mode or use 'adb' prefix (future).\n");
    }
    
    printf("\n");
    printf("Utility:\n");
    printf("  help, ?                  Show this help message\n");
    printf("  version                  Show version information\n");
    printf("  cls                      Clear screen\n");
    printf("  exit, quit               Exit program\n");
    printf("\n");
    printf("Note: Auto device monitoring is enabled by default (3s interval)\n");
}

// Parse command input
Command ParseCommand(const char* input) {
    Command cmd = {0};

    if (!input || strlen(input) == 0) {
        return cmd;
    }

    // Skip leading whitespace
    while (isspace(*input)) input++;

    // Find first space to separate command from args
    const char* space = strchr(input, ' ');
    if (space) {
        size_t name_len = space - input;
        if (name_len >= sizeof(cmd.name)) name_len = sizeof(cmd.name) - 1;
        memcpy(cmd.name, input, name_len);
        cmd.name[name_len] = '\0';

        // Copy args
        strncpy(cmd.args, space + 1, sizeof(cmd.args) - 1);
    } else {
        strncpy(cmd.name, input, sizeof(cmd.name) - 1);
    }

    // Trim and lowercase command name
    TrimString(cmd.name);
    TrimString(cmd.args);
    StringToLower(cmd.name);

    return cmd;
}

// Internal dispatcher for fastboot subcommands
static int DispatchFastbootSubcommand(AppState* state, const char* subcommand, const char* args) {
    Command subcmd = {0};
    snprintf(subcmd.name, sizeof(subcmd.name), "fb_%s", subcommand);
    if (args) {
        strncpy(subcmd.args, args, sizeof(subcmd.args) - 1);
    }

    if (strcmp(subcommand, "devices") == 0) {
        return CmdFbDevices(state, &subcmd);
    } else if (strcmp(subcommand, "select") == 0) {
        return CmdFbSelect(state, &subcmd);
    } else if (strcmp(subcommand, "info") == 0) {
        return CmdFbInfo(state, &subcmd);
    } else if (strcmp(subcommand, "flash") == 0) {
        return CmdFbFlash(state, &subcmd);
    } else if (strcmp(subcommand, "erase") == 0) {
        return CmdFbErase(state, &subcmd);
    } else if (strcmp(subcommand, "format") == 0) {
        return CmdFbFormat(state, &subcmd);
    } else if (strcmp(subcommand, "unlock") == 0) {
        return CmdFbUnlock(state, &subcmd);
    } else if (strcmp(subcommand, "lock") == 0) {
        return CmdFbLock(state, &subcmd);
    } else if (strcmp(subcommand, "oem") == 0) {
        return CmdFbOem(state, &subcmd);
    } else if (strcmp(subcommand, "reboot") == 0) {
        return CmdFbReboot(state, &subcmd);
    } else if (strcmp(subcommand, "getvar") == 0) {
        return CmdFbGetvar(state, &subcmd);
    } else if (strcmp(subcommand, "activate") == 0) {
        return CmdFbActivate(state, &subcmd);
    } else if (strcmp(subcommand, "wipe") == 0) {
        return CmdFbWipe(state, &subcmd);
    }
    
    return 0; // Not a fastboot command
}

// Internal dispatcher for ADB subcommands
static int DispatchAdbSubcommand(AppState* state, const char* subcommand, const char* args) {
    Command subcmd = {0};
    strncpy(subcmd.name, subcommand, sizeof(subcmd.name) - 1);
    if (args) {
        strncpy(subcmd.args, args, sizeof(subcmd.args) - 1);
    }

    if (strcmp(subcommand, "devices") == 0 || strcmp(subcommand, "dev") == 0) {
        return CmdDevices(state, &subcmd);
    } else if (strcmp(subcommand, "select") == 0) {
        return CmdSelect(state, &subcmd);
    } else if (strcmp(subcommand, "info") == 0) {
        return CmdInfo(state, &subcmd);
    } else if (strcmp(subcommand, "push") == 0) {
        return CmdPush(state, &subcmd);
    } else if (strcmp(subcommand, "pull") == 0) {
        return CmdPull(state, &subcmd);
    } else if (strcmp(subcommand, "ls") == 0) {
        return CmdLs(state, &subcmd);
    } else if (strcmp(subcommand, "rm") == 0) {
        return CmdRm(state, &subcmd);
    } else if (strcmp(subcommand, "mkdir") == 0) {
        return CmdMkdir(state, &subcmd);
    } else if (strcmp(subcommand, "shell") == 0) {
        return CmdShell(state, &subcmd);
    } else if (strcmp(subcommand, "sudo") == 0) {
        return CmdSudo(state, &subcmd);
    } else if (strcmp(subcommand, "install") == 0) {
        return CmdInstall(state, &subcmd);
    } else if (strcmp(subcommand, "uninstall") == 0) {
        return CmdUninstall(state, &subcmd);
    } else if (strcmp(subcommand, "reboot") == 0) {
        return CmdReboot(state, &subcmd);
    } else if (strcmp(subcommand, "dli") == 0) {
        return CmdDli(state, &subcmd);
    }

    return 0; // Not an ADB command
}

// Command: adb <command> [args...]
int CmdAdb(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        printf("Usage: adb <command> [args...]\n");
        printf("Commands: devices, select, info, push, pull, ls, rm, mkdir, shell, install, uninstall, reboot, dli\n");
        return 1;
    }

    char subcommand[64];
    const char* rest_args = NULL;

    // Parse subcommand
    const char* space = strchr(cmd->args, ' ');
    if (space) {
        size_t len = space - cmd->args;
        if (len >= sizeof(subcommand)) len = sizeof(subcommand) - 1;
        strncpy(subcommand, cmd->args, len);
        subcommand[len] = '\0';
        rest_args = space + 1;
        while (isspace(*rest_args)) rest_args++;
    } else {
        strncpy(subcommand, cmd->args, sizeof(subcommand) - 1);
        subcommand[sizeof(subcommand) - 1] = '\0';
        rest_args = "";
    }
    
    // Lowercase subcommand
    StringToLower(subcommand);

    if (DispatchAdbSubcommand(state, subcommand, rest_args)) {
        return 1;
    } else {
        printf("Unknown adb command: %s\n", subcommand);
        printf("Type 'adb' to see available commands.\n");
        return 1;
    }
}

// Command: fastboot <command> [args...]
int CmdFastboot(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        printf("Usage: fastboot <command> [args...]\n");
        printf("Commands: devices, select, info, flash, erase, format, unlock, lock, oem, reboot, getvar, activate, wipe\n");
        return 1;
    }

    char subcommand[64];
    const char* rest_args = NULL;

    // Parse subcommand
    const char* space = strchr(cmd->args, ' ');
    if (space) {
        size_t len = space - cmd->args;
        if (len >= sizeof(subcommand)) len = sizeof(subcommand) - 1;
        strncpy(subcommand, cmd->args, len);
        subcommand[len] = '\0';
        rest_args = space + 1;
        while (isspace(*rest_args)) rest_args++;
    } else {
        strncpy(subcommand, cmd->args, sizeof(subcommand) - 1);
        subcommand[sizeof(subcommand) - 1] = '\0';
        rest_args = "";
    }
    
    // Lowercase subcommand
    StringToLower(subcommand);

    if (DispatchFastbootSubcommand(state, subcommand, rest_args)) {
        return 1;
    } else {
        printf("Unknown fastboot command: %s\n", subcommand);
        printf("Type 'fastboot' to see available commands.\n");
        return 1;
    }
}

// Shortcut structure
typedef struct {
    const char* name;
    const char* cmd;
    const char* description;
} Shortcut;

static const Shortcut g_adb_shortcuts[] = {
    {"1", "devices", "List connected devices"},
    {"2", "info", "Show device information"},
    {"3", "shell", "Enter interactive shell"},
    {"4", "reboot", "Reboot system"},
    {"5", "reboot bootloader", "Reboot to bootloader"},
    {"6", "reboot recovery", "Reboot to recovery"},
    {"7", "install", "Install APK (requires path)"},
    {"8", "push", "Push file (requires local remote)"},
    {"9", "pull", "Pull file (requires remote [local])"},
    {NULL, NULL, NULL}
};

static const Shortcut g_fastboot_shortcuts[] = {
    {"1", "devices", "List fastboot devices"},
    {"2", "info", "Show device information"},
    {"3", "reboot", "Reboot system"},
    {"4", "reboot bootloader", "Reboot to bootloader"},
    {"5", "getvar all", "Get all variables"},
    {"6", "unlock", "Unlock bootloader"},
    {"7", "lock", "Lock bootloader"},
    {"8", "flash boot", "Flash boot partition"},
    {"9", "flash recovery", "Flash recovery partition"},
    {"10", "flash system", "Flash system partition"},
    {"11", "flash vbmeta", "Flash vbmeta partition"},
    {"12", "flash init_boot", "Flash init_boot partition"},
    {"13", "flash vendor_boot", "Flash vendor_boot partition"},
    {NULL, NULL, NULL}
};

// Track lines printed by ShowShortcuts
static int g_last_shortcuts_line_count = 0;

static void ShowShortcuts(AppState* state) {
    const Shortcut* shortcuts = (state->current_mode == MODE_FASTBOOT) ? g_fastboot_shortcuts : g_adb_shortcuts;
    int lines = 0;
    
    printf("\n"); lines++;
    printf("========================================\n"); lines++;
    printf("           Quick Selection\n"); lines++;
    printf("========================================\n"); lines++;
    
    for (int i = 0; shortcuts[i].name != NULL; i++) {
        printf("  %s. %s\n", shortcuts[i].name, shortcuts[i].description);
        lines++;
    }
    printf("========================================\n"); lines++;
    printf("Type the number and press Enter to auto-fill the command.\n"); lines++;

    g_last_shortcuts_line_count = lines;
}

static const char* GetShortcutCommand(AppState* state, const char* name) {
    const Shortcut* shortcuts = (state->current_mode == MODE_FASTBOOT) ? g_fastboot_shortcuts : g_adb_shortcuts;
    
    for (int i = 0; shortcuts[i].name != NULL; i++) {
        if (strcmp(name, shortcuts[i].name) == 0) {
            return shortcuts[i].cmd;
        }
    }
    return NULL;
}

// Execute command
int ExecuteCommand(AppState* state, const Command* cmd) {
    if (!state || !cmd) return 0;

    if (strlen(cmd->name) == 0) return 1;

    // Handle "s" for shortcuts menu
    if (strcmp(cmd->name, "s") == 0) {
        ShowShortcuts(state);
        return 1;
    }

    // 1. If in fastboot mode, try fastboot subcommands first (no prefix)
    if (state->current_mode == MODE_FASTBOOT) {
        // Exclude utility commands from auto-routing
        if (strcmp(cmd->name, "reboot") != 0 && 
            strcmp(cmd->name, "help") != 0 && 
            strcmp(cmd->name, "?") != 0 && 
            strcmp(cmd->name, "exit") != 0 && 
            strcmp(cmd->name, "quit") != 0 && 
            strcmp(cmd->name, "version") != 0) 
        {
            if (DispatchFastbootSubcommand(state, cmd->name, cmd->args)) {
                return 1;
            }
        }
    }

    // 2. If in ADB mode, try ADB subcommands first (no prefix)
    if (state->current_mode == MODE_ADB) {
        // Exclude utility commands from auto-routing
        if (strcmp(cmd->name, "reboot") != 0 && 
            strcmp(cmd->name, "help") != 0 && 
            strcmp(cmd->name, "?") != 0 && 
            strcmp(cmd->name, "exit") != 0 && 
            strcmp(cmd->name, "quit") != 0 && 
            strcmp(cmd->name, "version") != 0) 
        {
            if (DispatchAdbSubcommand(state, cmd->name, cmd->args)) {
                return 1;
            }
        }
    }

    // 3. Handle explicit prefix commands (adb, fastboot)
    if (strcmp(cmd->name, "adb") == 0) {
        return CmdAdb(state, cmd);
    } else if (strcmp(cmd->name, "fastboot") == 0) {
        return CmdFastboot(state, cmd);
    }

    // 4. Handle shared or utility commands
    if (strcmp(cmd->name, "reboot") == 0) {
        return CmdReboot(state, cmd);
    } else if (strcmp(cmd->name, "help") == 0 || strcmp(cmd->name, "?") == 0) {
        return CmdHelp(state, cmd);
    } else if (strcmp(cmd->name, "version") == 0) {
        return CmdVersion(state, cmd);
    } else if (strcmp(cmd->name, "cls") == 0) {
        return CmdCls(state, cmd);
    } else if (strcmp(cmd->name, "exit") == 0 || strcmp(cmd->name, "quit") == 0) {
        return -1; // Signal to exit
    }

    // 5. Fallback for no-prefix commands if they weren't caught by mode-specific dispatchers
    // (This allows calling ADB commands while in Fastboot mode without prefix if there's no name conflict)
    if (DispatchAdbSubcommand(state, cmd->name, cmd->args)) {
        return 1;
    }
    if (DispatchFastbootSubcommand(state, cmd->name, cmd->args)) {
        return 1;
    }

    // 6. Handle legacy fb_ prefixed commands
    if (strncmp(cmd->name, "fb_", 3) == 0) {
        if (DispatchFastbootSubcommand(state, cmd->name + 3, cmd->args)) {
            return 1;
        }
    }

    printf("Unknown command: %s\n", cmd->name);
    printf("Type 'help' for available commands.\n");
    return 1;
}

// Command: devices
int CmdDevices(AppState* state, const Command* cmd) {
    RefreshDeviceList(state);
    PrintDeviceList(state);
    return 1;
}

// Command: select
int CmdSelect(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: select <index|serial>");
        return 1;
    }

    // Try as index first
    if (isdigit(cmd->args[0])) {
        int index = atoi(cmd->args);
        if (SelectDevice(state, index)) {
            AdbDevice* dev = GetSelectedDevice(state);
            printf("Selected device: %s\n", dev->serial_id);
        } else {
            printf("Invalid device index.\n");
        }
    } else {
        // Try as serial number
        if (SelectDeviceBySerial(state, cmd->args)) {
            AdbDevice* dev = GetSelectedDevice(state);
            printf("Selected device: %s\n", dev->serial_id);
        } else {
            printf("Device not found: %s\n", cmd->args);
        }
    }

    return 1;
}

// Command: info
int CmdInfo(AppState* state, const Command* cmd) {
    AdbDevice* device = GetSelectedDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, NULL);
        return 1;
    }

    printf("\n");
    printf("========================================\n");
    printf("         Device Information\n");
    printf("========================================\n");
    printf("Serial ID:    %s\n", device->serial_id);
    printf("Model:        %s\n", strlen(device->model) > 0 ? device->model : "Unknown");
    printf("Device:       %s\n", strlen(device->device_name) > 0 ? device->device_name : "Unknown");
    printf("Status:       %s\n", device->status);
    if (strlen(device->android_version) > 0) {
        printf("Android:      %s (API %s)\n", device->android_version, device->api_level);
    }
    printf("========================================\n");

    return 1;
}

// Command: shell
int CmdShell(AppState* state, const Command* cmd) {
    AdbDevice* device = GetSelectedDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, NULL);
        return 1;
    }

    // If no arguments, enter interactive shell mode
    if (strlen(cmd->args) == 0) {
        // Prepare sudo script for shell
        #ifdef _WIN32
        _mkdir("tmp");
        #else
        mkdir("tmp", 0755);
        #endif

        FILE* fp = fopen("tmp\\sudo", "wb");
        if (fp) {
            fprintf(fp, "#!/system/bin/sh\nexec su -c \"$@\"\n");
            fclose(fp);

            // Push to device silently
            ProcessResult* res = AdbPushFile(state->adb_path, device->serial_id, "tmp\\sudo", "/data/local/tmp/sudo");
            if (res) FreeProcessResult(res);

            // Make executable
            res = AdbShellCommand(state->adb_path, device->serial_id, "chmod 755 /data/local/tmp/sudo");
            if (res) FreeProcessResult(res);
        }

        // Save current console mode
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwSavedInMode = 0, dwSavedOutMode = 0;

        if (hIn != INVALID_HANDLE_VALUE) {
            GetConsoleMode(hIn, &dwSavedInMode);
            // Restore QuickEdit mode and disable mouse input for shell
            // This enables right-click paste and other terminal features
            DWORD dwInMode = dwSavedInMode;
            dwInMode |= ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS;
            dwInMode &= ~ENABLE_MOUSE_INPUT;
            SetConsoleMode(hIn, dwInMode);
        }

        char command[2048];
        // Construct command: "adb_path" -s serial shell -t "export PATH=/data/local/tmp:$PATH; /system/bin/sh"
        // On Windows system(), if the command starts and ends with quotes, they are stripped.
        // We wrap the entire command in extra quotes to prevent this: ""path" args"
        snprintf(command, sizeof(command), "\"\"%s\" -s %s shell -t \"export PATH=/data/local/tmp:$PATH; /system/bin/sh\"\"", state->adb_path, device->serial_id);

        printf("Entering interactive shell mode with sudo support. Type 'exit' to return.\n");
        printf("----------------------------------------\n");

        // Execute system command to take over terminal
        system(command);

        printf("----------------------------------------\n");
        printf("Exited shell mode.\n");

        // Restore original console mode
        if (hIn != INVALID_HANDLE_VALUE) {
            SetConsoleMode(hIn, dwSavedInMode);
        }

        return 1;
    }

    ProcessResult* result = AdbShellCommand(state->adb_path, device->serial_id, cmd->args);
    if (!result) {
        PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to execute shell command");
        return 1;
    }

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    FreeProcessResult(result);
    return 1;
}

// Command: push
int CmdPush(AppState* state, const Command* cmd) {
    char local_path[MAX_PATH] = {0};
    char remote_path[MAX_PATH] = {0};

    if (sscanf(cmd->args, "%s %s", local_path, remote_path) != 2) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: push <local> <remote>");
        return 1;
    }

    return PushFile(state, local_path, remote_path);
}

// Command: pull
int CmdPull(AppState* state, const Command* cmd) {
    char remote_path[MAX_PATH] = {0};
    char local_path[MAX_PATH] = {0};

    int count = sscanf(cmd->args, "%s %s", remote_path, local_path);

    if (count < 1) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: pull <remote> [local]");
        return 1;
    }

    return PullFile(state, remote_path, count >= 2 ? local_path : NULL);
}

// Command: install
int CmdInstall(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: install <apk_file>");
        return 1;
    }

    AdbDevice* device = GetSelectedDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, NULL);
        return 1;
    }

    if (!FileExists(cmd->args)) {
        PrintError(ADB_ERROR_FILE_NOT_FOUND, cmd->args);
        return 1;
    }


    ProcessResult* result = AdbInstallApk(state->adb_path, device->serial_id, cmd->args);
    if (!result) {
        PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to install APK");
        return 1;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    if (success) {
        printf("APK installed successfully.\n");
    } else {
        PrintError(ADB_ERROR_UNKNOWN, "Failed to install APK");
    }

    FreeProcessResult(result);
    return 1;
}

// Command: uninstall
int CmdUninstall(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: uninstall <package_name>");
        return 1;
    }

    AdbDevice* device = GetSelectedDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, NULL);
        return 1;
    }

    printf("Uninstalling %s...\n", cmd->args);

    ProcessResult* result = AdbUninstallPackage(state->adb_path, device->serial_id, cmd->args);
    if (!result) {
        PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to uninstall package");
        return 1;
    }

    int success = (result->exit_code == 0);

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    if (success) {
        printf("Package uninstalled successfully.\n");
    } else {
        PrintError(ADB_ERROR_UNKNOWN, "Failed to uninstall package");
    }

    FreeProcessResult(result);
    return 1;
}

// Command: ls
int CmdLs(AppState* state, const Command* cmd) {
    const char* path = (strlen(cmd->args) > 0) ? cmd->args : "/sdcard";
    return ListRemoteFiles(state, path);
}

// Command: rm
int CmdRm(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: rm <remote_path>");
        return 1;
    }
    return DeleteRemoteFile(state, cmd->args);
}

// Command: mkdir
int CmdMkdir(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: mkdir <remote_path>");
        return 1;
    }
    return CreateRemoteDirectory(state, cmd->args);
}

// Command: sudo
int CmdSudo(AppState* state, const Command* cmd) {
    AdbDevice* device = GetSelectedDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, NULL);
        return 1;
    }

    if (strlen(cmd->args) == 0) {
        printf("Usage: sudo <command>\n");
        return 1;
    }

    const char* args[6];
    int idx = 0;

    args[idx++] = "-s";
    args[idx++] = device->serial_id;
    args[idx++] = "shell";
    args[idx++] = "su";
    args[idx++] = "-c";
    args[idx++] = cmd->args;

    ProcessResult* result = RunAdbCommand(state->adb_path, args, idx);
    if (!result) {
        PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to execute sudo command");
        return 1;
    }

    if (result->stdout_data && strlen(result->stdout_data) > 0) {
        printf("%s\n", result->stdout_data);
    }

    if (result->stderr_data && strlen(result->stderr_data) > 0) {
        fprintf(stderr, "%s\n", result->stderr_data);
    }

    FreeProcessResult(result);
    return 1;
}

// Command: reboot
int CmdReboot(AppState* state, const Command* cmd) {
    // Check current mode and route to appropriate reboot function
    if (state->current_mode == MODE_FASTBOOT) {
        // In fastboot mode, use fastboot reboot
        TrimString((char*)cmd->args);
        return RebootFastbootDevice(state, strlen(cmd->args) > 0 ? cmd->args : NULL);
    } else {
        // In ADB mode, use ADB reboot
        AdbDevice* device = GetSelectedDevice(state);
        if (!device) {
            PrintError(ADB_ERROR_NO_DEVICE, NULL);
            return 1;
        }

        const char* mode = (strlen(cmd->args) > 0) ? cmd->args : "system";

        printf("Rebooting device to %s mode...\n", mode);

        ProcessResult* result = AdbReboot(state->adb_path, device->serial_id, mode);
        if (!result) {
            PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to reboot device");
            return 1;
        }

        int success = (result->exit_code == 0);

        if (success) {
            printf("Device is rebooting...\n");
        } else {
            PrintError(ADB_ERROR_UNKNOWN, "Failed to reboot device");
        }

        FreeProcessResult(result);
        return 1;
    }
}

// Command: help
int CmdHelp(AppState* state, const Command* cmd) {
    ShowHelp(state);
    return 1;
}

// Command: version
int CmdVersion(AppState* state, const Command* cmd) {
    printf("\n");
    printf("ADB Tool v%s\n", APP_VERSION);
    printf("Portable Android Debug Bridge Management Tool\n");
    printf("\n");

    // Also show ADB version
    const char* args[] = { "version" };
    ProcessResult* result = RunAdbCommand(state->adb_path, args, 1);
    if (result && result->stdout_data) {
        printf("Embedded ADB version:\n");
        printf("%s\n", result->stdout_data);
    }

    if (result) FreeProcessResult(result);
    return 1;
}

// Command: cls
int CmdCls(AppState* state, const Command* cmd) {
    system("cls");
    ShowBanner();
    return 1;
}

// Count APK files in input
int CountApks(const char* input) {
    if (!input || !*input) return 0;
    
    // Check if input starts with potential file indicators
    const char* trimmed = input;
    while (isspace(*trimmed)) trimmed++;
    
    int is_path = 0;
    if (trimmed[0] == '"') is_path = 1;
    else if (isalpha(trimmed[0]) && trimmed[1] == ':' && (trimmed[2] == '\\' || trimmed[2] == '/')) is_path = 1;
    else if (trimmed[0] == '/' || trimmed[0] == '\\') is_path = 1;
    
    if (!is_path) return 0;

    int count = 0;
    const char* cursor = trimmed;
    
    while (*cursor) {
        while (isspace(*cursor)) cursor++;
        if (*cursor == '\0') break;

        char file_path[MAX_PATH] = {0};
        int in_quotes = 0;
        int i = 0;

        if (*cursor == '"') {
            in_quotes = 1;
            cursor++;
        }

        while (*cursor && i < MAX_PATH - 1) {
            if (in_quotes) {
                if (*cursor == '"') {
                    cursor++;
                    break;
                }
            } else {
                if (isspace(*cursor)) {
                    break;
                }
            }
            file_path[i++] = *cursor++;
        }
        file_path[i] = '\0';
        
        if (strlen(file_path) > 0) {
            // Check for .apk extension
            const char* ext = strrchr(file_path, '.');
            if (ext && (_stricmp(ext, ".apk") == 0)) {
                count++;
            }
        }
    }
    return count;
}

// Handle drag and drop input (file paths)
int HandleDragDropInput(AppState* state, char* input) {
    // Check if input looks like a file path (starts with " or drive letter)
    // and contains .apk
    char* trimmed = input;
    while (isspace(*trimmed)) trimmed++;

    // heuristic: starts with " or X:\ or /
    int is_path = 0;
    if (trimmed[0] == '"') is_path = 1;
    else if (isalpha(trimmed[0]) && trimmed[1] == ':' && (trimmed[2] == '\\' || trimmed[2] == '/')) is_path = 1;
    else if (trimmed[0] == '/' || trimmed[0] == '\\') is_path = 1;

    if (!is_path) return 0;

    // Check if it contains .apk (case insensitive)
    int has_apk = 0;
    char* p = trimmed;
    while (*p) {
        if (tolower(p[0]) == '.' && tolower(p[1]) == 'a' && tolower(p[2]) == 'p' && tolower(p[3]) == 'k') {
            has_apk = 1;
            break;
        }
        p++;
    }

    if (!has_apk) return 0;

    // Check mode
    if (state->current_mode == MODE_FASTBOOT) {
        printf("\nError: Cannot install APK in fastboot mode.\n");
        printf("Please switch to ADB mode first.\n");
        return 1; // Handled (as error)
    }

    // It looks like file(s). Parse and install each.
    printf("\nDetected APK drag-and-drop. Starting installation...\n");

    char* cursor = trimmed;
    while (*cursor) {
        // Skip leading whitespace
        while (isspace(*cursor)) cursor++;
        if (*cursor == '\0') break;

        char file_path[MAX_PATH] = {0};
        int in_quotes = 0;
        int i = 0;

        if (*cursor == '"') {
            in_quotes = 1;
            cursor++; // skip opening quote
        }

        while (*cursor && i < MAX_PATH - 1) {
            if (in_quotes) {
                if (*cursor == '"') {
                    cursor++; // skip closing quote
                    break;
                }
            } else {
                if (isspace(*cursor)) {
                    break;
                }
            }
            file_path[i++] = *cursor++;
        }
        file_path[i] = '\0';

        // Process this file
        if (strlen(file_path) > 0) {
            // Check if it is an APK
            char* ext = strrchr(file_path, '.');
            if (ext && (_stricmp(ext, ".apk") == 0)) {
                printf("\nInstalling: %s\n", file_path);
                
                // Construct command for CmdInstall
                Command cmd = {0};
                strcpy(cmd.name, "install");
                strncpy(cmd.args, file_path, sizeof(cmd.args) - 1);
                
                CmdInstall(state, &cmd);
            } else {
                printf("\nSkipping non-APK file: %s\n", file_path);
            }
        }
    }
    
    return 1; // Handled
}

// Define missing constant if needed
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

// Clear current line using Windows API or fallback
void ClearLine() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        DWORD written;
        COORD start = {0, csbi.dwCursorPosition.Y};
        FillConsoleOutputCharacterA(hOut, ' ', csbi.dwSize.X, start, &written);
        SetConsoleCursorPosition(hOut, start);
    } else {
        printf("\r                                                                                \r");
    }
}

// Run interactive loop
void RunInteractiveLoop(AppState* state) {
    char input[4096] = {0};
    int input_pos = 0;
    int prompt_shown = 0;

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    
    if (hIn == INVALID_HANDLE_VALUE || hOut == INVALID_HANDLE_VALUE) return;

    // Enable VT mode and Mouse Input
    DWORD dwOutMode = 0;
    if (GetConsoleMode(hOut, &dwOutMode)) {
        dwOutMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwOutMode);
    }

    DWORD dwInMode = 0;
    if (GetConsoleMode(hIn, &dwInMode)) {
        // ENABLE_EXTENDED_FLAGS is needed to disable QuickEdit mode 
        // which can interfere with mouse events
        dwInMode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
        dwInMode &= ~ENABLE_QUICK_EDIT_MODE;
        SetConsoleMode(hIn, dwInMode);
    }

    // Register callback for prompt refresh
    SetPromptRefreshCallback(RefreshPromptCallback);

    g_current_history_view = -1;

    while (1) {
        // Show prompt if not shown or needs refresh
        if (!prompt_shown || g_prompt_needs_refresh) {
            if (g_prompt_needs_refresh && prompt_shown) {
                ClearLine();
                char prompt[256];
                GetPromptString(state, prompt, sizeof(prompt));
                printf("%s", prompt);
            } else {
                DisplayPrompt(state);
            }
            
            prompt_shown = 1;
            g_prompt_needs_refresh = 0;
            
            // If we have input in buffer, reprint it
            if (input_pos > 0) {
                input[input_pos] = '\0';
                int apks = CountApks(input);
                if (apks > 0) printf("%d APKs selected. Press Enter to install.", apks);
                else printf("%s", input);
            }
            fflush(stdout);
        }

        // Use PeekConsoleInput to check for events without blocking
        DWORD numEvents = 0;
        if (GetNumberOfConsoleInputEvents(hIn, &numEvents) && numEvents > 0) {
            INPUT_RECORD ir[32];
            DWORD read;
            if (ReadConsoleInput(hIn, ir, 32, &read)) {
                int needs_repaint = 0;
                for (DWORD i = 0; i < read; i++) {
                    if (ir[i].EventType == KEY_EVENT && ir[i].Event.KeyEvent.bKeyDown) {
                        int ch = ir[i].Event.KeyEvent.uChar.AsciiChar;
                        int vk = ir[i].Event.KeyEvent.wVirtualKeyCode;

                        // Handle Enter key
                        if (vk == VK_RETURN) {
                            printf("\n");
                            input[input_pos] = '\0';
                            
                            int shortcut_handled = 0;

                            // Check for shortcut
                            if (strlen(input) > 0 && isdigit(input[0])) {
                                const char* shortcut_cmd = GetShortcutCommand(state, input);
                                if (shortcut_cmd) {
                                    shortcut_handled = 1;

                                    // Clear shortcut menu if it was shown
                                    if (g_last_shortcuts_line_count > 0) {
                                        // Move up and clear
                                        // g_last_shortcuts_line_count (list) + 3 (newlines/prompts)
                                        printf("\033[%dA", g_last_shortcuts_line_count + 3);
                                        printf("\033[J");
                                        g_last_shortcuts_line_count = 0;
                                    }

                                    // Replace input with shortcut command
                                    strncpy(input, shortcut_cmd, sizeof(input) - 1);
                                    // Append space for convenience
                                    strncat(input, " ", sizeof(input) - strlen(input) - 1);
                                    input_pos = (int)strlen(input);
                                    
                                    // Repaint prompt with new input
                                    ClearLine();
                                    char prompt_str[256];
                                    GetPromptString(state, prompt_str, sizeof(prompt_str));
                                    printf("%s%s", prompt_str, input);
                                    fflush(stdout);
                                    
                                    // Do not execute, wait for user input
                                    needs_repaint = 0;
                                    break; 
                                }
                            }

                            if (!shortcut_handled) {
                                g_last_shortcuts_line_count = 0;
                            }

                            if (strlen(input) > 0) {
                                AddToHistory(input);
                                g_current_history_view = -1;
                            }

                            // Try to handle as drag-and-drop first
                            if (!HandleDragDropInput(state, input)) {
                                // Parse and execute as normal command
                                Command cmd = ParseCommand(input);
                                int result = ExecuteCommand(state, &cmd);
                                if (result == -1) {
                                    printf("Goodbye!\n");
                                    return;
                                }
                            }
                            
                            input_pos = 0;
                            input[0] = '\0';
                            prompt_shown = 0;
                            needs_repaint = 0;
                            break; 
                        }
                        // Handle Backspace
                        else if (vk == VK_BACK) {
                            if (input_pos > 0) {
                                input_pos--;
                                input[input_pos] = '\0';
                                needs_repaint = 1;
                            }
                        }
                        // Handle Up/Down Arrows
                        else if (vk == VK_UP || vk == VK_DOWN) {
                            if (g_history_count > 0) {
                                if (vk == VK_UP) {
                                    if (g_current_history_view == -1) g_current_history_view = g_history_count - 1;
                                    else if (g_current_history_view > 0) g_current_history_view--;
                                } else {
                                    if (g_current_history_view != -1) {
                                        if (g_current_history_view < g_history_count - 1) g_current_history_view++;
                                        else g_current_history_view = -1;
                                    }
                                }

                                if (g_current_history_view != -1) {
                                    strncpy(input, g_history[g_current_history_view % MAX_HISTORY], sizeof(input) - 1);
                                    input_pos = (int)strlen(input);
                                } else {
                                    input[0] = '\0';
                                    input_pos = 0;
                                }
                                needs_repaint = 1;
                            }
                        }
                        // Handle Normal Chars
                        else if (ch >= 32 && ch < 127) {
                            if (input_pos < (int)sizeof(input) - 1) {
                                input[input_pos++] = (char)ch;
                                input[input_pos] = '\0';
                                needs_repaint = 1;
                            }
                        }
                    }
                    // HANDLE MOUSE WHEEL (Scroll terminal buffer)
                    else if (ir[i].EventType == MOUSE_EVENT) {
                        if (ir[i].Event.MouseEvent.dwEventFlags & MOUSE_WHEELED) {
                            short scroll = (short)HIWORD(ir[i].Event.MouseEvent.dwButtonState);
                            
                            CONSOLE_SCREEN_BUFFER_INFO csbi;
                            if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
                                SMALL_RECT scrollRect = csbi.srWindow;
                                // Determine how many lines to scroll (standard is 3 lines per notch)
                                int linesToScroll = (scroll > 0) ? -3 : 3;
                                
                                // Adjust the window position
                                short newTop = scrollRect.Top + (short)linesToScroll;
                                short newBottom = scrollRect.Bottom + (short)linesToScroll;
                                
                                // Ensure we stay within buffer bounds
                                if (newTop < 0) {
                                    newBottom -= newTop;
                                    newTop = 0;
                                }
                                if (newBottom >= csbi.dwSize.Y) {
                                    newTop -= (newBottom - csbi.dwSize.Y + 1);
                                    newBottom = csbi.dwSize.Y - 1;
                                }

                                scrollRect.Top = newTop;
                                scrollRect.Bottom = newBottom;
                                SetConsoleWindowInfo(hOut, TRUE, &scrollRect);
                            }
                        }
                    }
                }

                if (needs_repaint) {
                    ClearLine();
                    char prompt_str[256];
                    GetPromptString(state, prompt_str, sizeof(prompt_str));
                    printf("%s", prompt_str);
                    
                    int apks = CountApks(input);
                    if (apks > 0) printf("%d APKs selected. Press Enter to install.", apks);
                    else printf("%s", input);
                    
                    fflush(stdout);
                }
            }
        }
        
        Sleep(10);
    }
}

// ============================================================================
// Fastboot Command Handlers
// ============================================================================

// Command: fb_devices
int CmdFbDevices(AppState* state, const Command* cmd) {
    RefreshFastbootDeviceList(state);
    PrintFastbootDeviceList(state);
    return 1;
}

// Command: fb_select
int CmdFbSelect(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: fb_select <index|serial>");
        return 1;
    }

    // Try as index first
    if (isdigit(cmd->args[0])) {
        int index = atoi(cmd->args);
        if (SelectFastbootDevice(state, index)) {
            AdbDevice* dev = GetSelectedFastbootDevice(state);
            printf("Selected fastboot device: %s\n", dev->serial_id);
            // Switch to fastboot mode
            state->current_mode = MODE_FASTBOOT;
        } else {
            printf("Invalid fastboot device index.\n");
        }
    } else {
        // Try as serial number
        if (SelectFastbootDeviceBySerial(state, cmd->args)) {
            AdbDevice* dev = GetSelectedFastbootDevice(state);
            printf("Selected fastboot device: %s\n", dev->serial_id);
            // Switch to fastboot mode
            state->current_mode = MODE_FASTBOOT;
        } else {
            printf("Fastboot device not found: %s\n", cmd->args);
        }
    }

    return 1;
}

// Command: fb_info
int CmdFbInfo(AppState* state, const Command* cmd) {
    ShowFastbootDeviceInfo(state);
    return 1;
}

// Command: fb_flash
int CmdFbFlash(AppState* state, const Command* cmd) {
    char partition[64] = {0};
    char image_path[MAX_PATH] = {0};

    if (sscanf(cmd->args, "%s %s", partition, image_path) != 2) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: flash <partition> <image_file>");
        return 1;
    }

    // Switch to fastboot mode
    state->current_mode = MODE_FASTBOOT;
    return FlashImage(state, partition, image_path);
}

// Command: fb_erase
int CmdFbErase(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: erase <partition>");
        return 1;
    }

    state->current_mode = MODE_FASTBOOT;
    return ErasePartition(state, cmd->args);
}

// Command: fb_format
int CmdFbFormat(AppState* state, const Command* cmd) {
    char partition[64] = {0};
    char fs_type[32] = {0};

    if (sscanf(cmd->args, "%s %s", partition, fs_type) != 2) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: format <partition> <filesystem>");
        return 1;
    }

    state->current_mode = MODE_FASTBOOT;
    return FormatPartition(state, partition, fs_type);
}

// Command: fb_unlock
int CmdFbUnlock(AppState* state, const Command* cmd) {
    state->current_mode = MODE_FASTBOOT;
    return UnlockBootloader(state);
}

// Command: fb_lock
int CmdFbLock(AppState* state, const Command* cmd) {
    state->current_mode = MODE_FASTBOOT;
    return LockBootloader(state);
}

// Command: fb_oem
int CmdFbOem(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: oem <command>");
        return 1;
    }

    state->current_mode = MODE_FASTBOOT;
    return ExecuteOemCommand(state, cmd->args);
}

// Command: fb_reboot
int CmdFbReboot(AppState* state, const Command* cmd) {
    TrimString((char*)cmd->args);
    state->current_mode = MODE_FASTBOOT;
    return RebootFastbootDevice(state, strlen(cmd->args) > 0 ? cmd->args : NULL);
}

// Command: fb_getvar
int CmdFbGetvar(AppState* state, const Command* cmd) {
    TrimString((char*)cmd->args);
    state->current_mode = MODE_FASTBOOT;
    return GetFastbootVar(state, strlen(cmd->args) > 0 ? cmd->args : NULL);
}

// Command: fb_activate
int CmdFbActivate(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: activate <slot>");
        return 1;
    }

    state->current_mode = MODE_FASTBOOT;
    return ActivateFastbootSlot(state, cmd->args);
}

// Command: fb_wipe
int CmdFbWipe(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: wipe <partition>");
        return 1;
    }

    state->current_mode = MODE_FASTBOOT;
    return WipeFastbootPartition(state, cmd->args);
}

// Helper to extract filename from URL
static void GetFilenameFromUrl(const char* url, char* buffer, size_t size) {
    const char* last_slash = strrchr(url, '/');
    const char* last_backslash = strrchr(url, '\\');
    
    const char* filename = url;
    if (last_slash && last_slash > last_backslash) filename = last_slash + 1;
    else if (last_backslash) filename = last_backslash + 1;

    // Handle query parameters if present
    char temp[MAX_PATH];
    strncpy(temp, filename, sizeof(temp)-1);
    char* question_mark = strchr(temp, '?');
    if (question_mark) *question_mark = '\0';
    
    // If empty or invalid, use default
    if (strlen(temp) == 0) {
        strcpy(buffer, "downloaded_module.zip");
    } else {
        strncpy(buffer, temp, size - 1);
        buffer[size - 1] = '\0';
    }
}

// Command: dli <url>
int CmdDli(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: dli <url>");
        return 1;
    }

    const char* url = cmd->args;
    
    // Create tmp directory
    #ifdef _WIN32
    _mkdir("tmp");
    #else
    mkdir("tmp", 0755);
    #endif

    char filename[MAX_PATH];
    GetFilenameFromUrl(url, filename, sizeof(filename));

    char local_path[MAX_PATH];
    snprintf(local_path, sizeof(local_path), "tmp\\%s", filename);

    // 1. Download
    printf("Downloading module from %s...\n", url);
    if (!DownloadFile(url, local_path)) {
        printf("Error: Failed to download file.\n");
        return 1;
    }

    // 2. Push to device
    AdbDevice* dev = GetSelectedDevice(state);
    if (!dev) {
        printf("Error: No device selected. Cannot install module.\n");
        return 1;
    }

    char remote_path[MAX_PATH];
    snprintf(remote_path, sizeof(remote_path), "/storage/emulated/0/%s", filename);

    printf("Pushing to device: %s -> %s\n", local_path, remote_path);
    if (!PushFile(state, local_path, remote_path)) {
        printf("Error: Failed to push file to device.\n");
        return 1;
    }

    // 3. Install Module
    // Check if it's a module zip first? 
    // User logic: "download -> push -> install module system install module"
    // We should probably verify it's a module first locally using existing logic
    char seven_zip_path[MAX_PATH];
    snprintf(seven_zip_path, sizeof(seven_zip_path), "%s\\7za.exe", state->temp_dir);
    if (!IsModuleZip(local_path, seven_zip_path)) {
        printf("Warning: Downloaded file does not appear to be a Magisk/KSU/APatch module (no module.prop).\n");
        printf("Proceeding with installation anyway as requested...\n");
    }

    RootSolution solution = DetectRootSolution(state);
    if (solution == ROOT_NONE) {
        printf("Error: No supported root solution (Magisk/KSU/APatch) detected on device.\n");
        return 1;
    }

    InstallRootModule(state, remote_path, solution);

    return 1;
}
