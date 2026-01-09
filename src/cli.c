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

// Forward declaration for helper function
static int CountNewlines(const char* str);

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
    char device_str[64] = "no device";
    const char* mode_str = (state->current_mode == MODE_FASTBOOT) ? "fastboot" : "adb";
    
    // Get device ID
    if (state->current_mode == MODE_FASTBOOT) {
        AdbDevice* device = GetSelectedFastbootDevice(state);
        if (device) strncpy(device_str, device->serial_id, sizeof(device_str)-1);
    } else {
        AdbDevice* device = GetSelectedDevice(state);
        if (device) strncpy(device_str, device->serial_id, sizeof(device_str)-1);
    }

    switch (state->current_theme) {
        case THEME_ROBBYRUSSELL:
            // ➜  mode git:(device) ✗
            // Using simplified ASCII arrow ->
            // Color scheme: Arrow (green/red), mode (cyan), device (blue)
            snprintf(buffer, size, ANSI_BOLD ANSI_GREEN "-> " ANSI_RESET ANSI_CYAN "%s" ANSI_RESET " device:(" ANSI_BRIGHT_BLUE "%s" ANSI_RESET ") " ANSI_YELLOW ">>> " ANSI_RESET, mode_str, device_str);
            break;
            
        case THEME_AGNOSTER:
            // mode | device >
            // Blue background for context
            snprintf(buffer, size, ANSI_BG_BLUE ANSI_BOLD ANSI_WHITE " %s | %s " ANSI_RESET ANSI_BLUE "\x10" ANSI_RESET " ", mode_str, device_str);
            // Fallback if \x10 (triangle) doesn't look right: >
            // snprintf(buffer, size, ANSI_BG_BLUE ANSI_BOLD ANSI_WHITE " %s | %s " ANSI_RESET ANSI_BLUE ">" ANSI_RESET " ", mode_str, device_str);
            break;

        case THEME_MINIMAL:
            // Minimal: device >
            snprintf(buffer, size, ANSI_CYAN "%s" ANSI_RESET " " ANSI_BOLD ANSI_MAGENTA "> " ANSI_RESET, device_str);
            break;

        case THEME_PURE:
            // Pure: Two-line prompt
            // mode device
            // >
            // Remove leading \n completely, handled by DisplayPrompt logic if needed
            snprintf(buffer, size, ANSI_BOLD ANSI_MAGENTA "%s" ANSI_RESET " " ANSI_CYAN "%s" ANSI_RESET "\n" ANSI_BOLD ANSI_MAGENTA "> " ANSI_RESET, mode_str, device_str);
            break;

        case THEME_NEON:
            // Neon: [mode] device >> (Bright colors)
            snprintf(buffer, size, ANSI_BRIGHT_MAGENTA "[" ANSI_BRIGHT_CYAN "%s" ANSI_BRIGHT_MAGENTA "]" ANSI_RESET " " ANSI_BRIGHT_GREEN "%s" ANSI_RESET ANSI_BRIGHT_YELLOW " >> " ANSI_RESET, mode_str, device_str);
            break;

        case THEME_DRACULA:
            // Dracula: purple/pink theme
            snprintf(buffer, size, ANSI_MAGENTA "%s" ANSI_RESET " " ANSI_BRIGHT_MAGENTA "\x10" ANSI_RESET " " ANSI_CYAN "%s" ANSI_RESET " " ANSI_BRIGHT_GREEN "$ " ANSI_RESET, mode_str, device_str);
            break;

        case THEME_MATRIX:
            // Matrix: All green
            snprintf(buffer, size, ANSI_GREEN "%s@%s: " ANSI_BOLD "> " ANSI_RESET, mode_str, device_str);
            break;

        case THEME_CYBERPUNK:
            // Cyberpunk: Yellow/Blue/Red
            snprintf(buffer, size, ANSI_BG_BLUE ANSI_YELLOW " %s " ANSI_BG_RESET ANSI_BLUE "\x10" ANSI_RESET " " ANSI_RED "%s" ANSI_RESET ANSI_BRIGHT_YELLOW " > " ANSI_RESET, mode_str, device_str);
            break;

        case THEME_SUNSET:
            // Sunset: Red/Magenta/Yellow
            snprintf(buffer, size, ANSI_RED "%s" ANSI_RESET ANSI_MAGENTA " ~ " ANSI_RESET ANSI_YELLOW "%s" ANSI_RESET " $ ", mode_str, device_str);
            break;

        case THEME_FOREST:
            // Forest: Green/Yellow
            snprintf(buffer, size, ANSI_GREEN "%s" ANSI_RESET ANSI_BRIGHT_GREEN " >> " ANSI_RESET ANSI_YELLOW "%s" ANSI_RESET " > ", mode_str, device_str);
            break;

        case THEME_OCEAN:
            // Ocean: Blue/Cyan
            snprintf(buffer, size, ANSI_BLUE "%s" ANSI_RESET ANSI_CYAN " :: " ANSI_RESET ANSI_BRIGHT_BLUE "%s" ANSI_RESET " ~> ", mode_str, device_str);
            break;

        case THEME_RETRO:
            // Retro: Amber color (using yellow)
            snprintf(buffer, size, ANSI_YELLOW "C:\\%s\\%s> " ANSI_RESET, mode_str, device_str);
            break;

        case THEME_MONOKAI:
            // Monokai: Pink/Green/Blue
            // Removed special char to fix encoding
            snprintf(buffer, size, ANSI_MAGENTA "%s" ANSI_RESET " " ANSI_GREEN "%s" ANSI_RESET " " ANSI_CYAN ">" ANSI_RESET " ", mode_str, device_str);
            break;

        case THEME_UBUNTU:
            // Ubuntu: user@host:~$ 
            // Green user, White @, Green host, Blue path, White $
            snprintf(buffer, size, ANSI_GREEN "user" ANSI_RESET "@" ANSI_GREEN "%s" ANSI_RESET ":" ANSI_BLUE "~" ANSI_RESET "$ ", device_str);
            break;

        case THEME_KALI:
            // Kali: ┌──(user㉿host)-[~]
            //       └─$
            snprintf(buffer, size, ANSI_BLUE "┌──(" ANSI_BOLD ANSI_RED "%s" ANSI_RESET ANSI_BLUE ")" ANSI_RESET "-" ANSI_BLUE "[~]" ANSI_RESET "\n" ANSI_BLUE "└─" ANSI_BLUE "$" ANSI_RESET " ", device_str);
            break;

        case THEME_ARCH:
            // Arch: user@arch ~ %
            snprintf(buffer, size, ANSI_CYAN "%s" ANSI_RESET "@" ANSI_BOLD "arch" ANSI_RESET " " ANSI_MAGENTA "~" ANSI_RESET " %% ", device_str);
            break;

        case THEME_GENTOO:
            // Gentoo: root@gentoo ~ #
            snprintf(buffer, size, ANSI_RED "root" ANSI_RESET "@" ANSI_GREEN "%s" ANSI_RESET " " ANSI_BLUE "~" ANSI_RESET " # ", device_str);
            break;

        case THEME_ROOT:
            // Root: # 
            snprintf(buffer, size, ANSI_BOLD ANSI_RED "# " ANSI_RESET);
            break;

        case THEME_DEBIAN:
            // Debian: user@debian:~$ (Red/White)
            snprintf(buffer, size, ANSI_RED "🌀" ANSI_RESET " " ANSI_GREEN "user" ANSI_RESET "@" ANSI_RED "debian" ANSI_RESET ":" ANSI_BLUE "~" ANSI_RESET "$ ");
            break;

        case THEME_FEDORA:
            // Fedora: [user@fedora ~]$ (Blue/White)
            snprintf(buffer, size, "[" ANSI_BLUE "user" ANSI_RESET "@" ANSI_BLUE "fedora" ANSI_RESET " " ANSI_WHITE "%s" ANSI_RESET "]$ ", device_str);
            break;

        case THEME_CENTOS:
            // CentOS: [user@centos ~]# (Green/Yellow/Blue)
            snprintf(buffer, size, "[" ANSI_GREEN "user" ANSI_RESET "@" ANSI_YELLOW "centos" ANSI_RESET " " ANSI_BLUE "%s" ANSI_RESET "]# ", device_str);
            break;

        case THEME_MANJARO:
            // Manjaro: [user@manjaro ~]$ (Green/Teal)
            snprintf(buffer, size, "[" ANSI_GREEN "user" ANSI_RESET "@" ANSI_GREEN "manjaro" ANSI_RESET " " ANSI_CYAN "%s" ANSI_RESET "]$ ", device_str);
            break;

        case THEME_MINT:
            // Mint: user@mint ~ $ (Green)
            snprintf(buffer, size, ANSI_GREEN "mint" ANSI_RESET "@" ANSI_GREEN "%s" ANSI_RESET " " ANSI_BLUE "~" ANSI_RESET " $ ", device_str);
            break;

        case THEME_ALPINE:
            // Alpine: alpine:~$ (Blue)
            snprintf(buffer, size, ANSI_BLUE "alpine" ANSI_RESET ":" ANSI_CYAN "%s" ANSI_RESET "$ ", device_str);
            break;

        case THEME_STARWARS:
            // StarWars: empire@deathstar:~$ 
            snprintf(buffer, size, ANSI_RED "empire" ANSI_RESET "@" ANSI_BOLD "deathstar" ANSI_RESET ":" ANSI_BLUE "%s" ANSI_RESET "$ ", device_str);
            break;

        case THEME_HACKER:
            // Hacker: root@mainframe:/access# (Green Matrix style)
            snprintf(buffer, size, ANSI_BOLD ANSI_GREEN "root@mainframe" ANSI_RESET ":" ANSI_GREEN "/%s" ANSI_RESET "# ", device_str);
            break;

        case THEME_GLITCH:
            // Glitch: [?_?] user@sys ~ $
            snprintf(buffer, size, ANSI_MAGENTA "[?_?]" ANSI_RESET " " ANSI_CYAN "sys" ANSI_RESET "@" ANSI_YELLOW "%s" ANSI_RESET " $ ", device_str);
            break;

        case THEME_ALIEN:
            // Alien: 👽 user@ufo:~$
            snprintf(buffer, size, "👽 " ANSI_GREEN "alien" ANSI_RESET "@" ANSI_GREEN "%s" ANSI_RESET ":" ANSI_BLUE "~" ANSI_RESET "$ ", device_str);
            break;

        case THEME_MACOS:
            // MacOS: user@MacBook ~ %
            snprintf(buffer, size, ANSI_GREEN "user" ANSI_RESET "@" ANSI_BOLD "%s" ANSI_RESET " " ANSI_BLUE "~" ANSI_RESET " %% ", device_str);
            break;

        case THEME_FREEBSD:
            // FreeBSD: user@freebsd:~ $ (Red/White)
            snprintf(buffer, size, ANSI_RED "root" ANSI_RESET "@" ANSI_WHITE "freebsd" ANSI_RESET ":" ANSI_BLUE "~" ANSI_RESET " # ");
            break;

        case THEME_SOLARIS:
            // Solaris: user@solaris:~ $ 
            snprintf(buffer, size, ANSI_YELLOW "%s" ANSI_RESET ":" ANSI_CYAN "~" ANSI_RESET " $ ", device_str);
            break;

        case THEME_WINDOWS11:
            // Windows 11 PowerShell: PS C:\adb>
            snprintf(buffer, size, ANSI_BLUE "PS" ANSI_RESET " " ANSI_YELLOW "C:\\adb\\%s" ANSI_RESET "> ", device_str);
            break;

        case THEME_MSDOS:
            // MS-DOS: C:\ADB>
            snprintf(buffer, size, "C:\\ADB\\%s> ", device_str);
            break;

        case THEME_FISH:
            // Fish: user@host ~>
            snprintf(buffer, size, ANSI_GREEN "user" ANSI_RESET "@" ANSI_YELLOW "%s" ANSI_RESET " " ANSI_BLUE "~" ANSI_RESET "> ", device_str);
            break;

        case THEME_ZSH_SIMPLE:
            // Zsh Simple: %
            snprintf(buffer, size, ANSI_BOLD "%% " ANSI_RESET);
            break;

        case THEME_BASH_SIMPLE:
            // Bash Simple: bash-5.2$
            snprintf(buffer, size, "bash-5.2$ ");
            break;

        case THEME_CLOUD:
            // Cloud: ☁️  user@cloud:dir $
            snprintf(buffer, size, "☁️  " ANSI_CYAN "user" ANSI_RESET "@" ANSI_MAGENTA "cloud" ANSI_RESET ":" ANSI_BLUE "~" ANSI_RESET " $ ");
            break;

        case THEME_IRONMAN:
            // IronMan: 🦾 JARVIS@Mark85:~$ (Red/Gold)
            snprintf(buffer, size, "🦾 " ANSI_RED "JARVIS" ANSI_RESET "@" ANSI_YELLOW "Mark85" ANSI_RESET ":" ANSI_CYAN "%s" ANSI_RESET " $ ", device_str);
            break;

        case THEME_POWERLEVEL:
            // Powerlevel: Complex powerline
            snprintf(buffer, size, ANSI_BG_BLUE ANSI_WHITE " %s " ANSI_BG_RESET ANSI_BLUE "\x10" ANSI_BG_RESET " " ANSI_CYAN "%s" ANSI_RESET " \x10 ", mode_str, device_str);
            break;

        case THEME_DEFAULT:
        default:
            snprintf(buffer, size, ANSI_GREEN "%s" ANSI_RESET " [" ANSI_YELLOW "%s" ANSI_RESET "]> ", mode_str, device_str);
            break;
    }
}

// Display command prompt
void DisplayPrompt(const AppState* state) {
    char buffer[256];
    GetPromptString(state, buffer, sizeof(buffer));
    
    // Check if theme is multiline (Pure, Kali)
    // Multiline prompts might need a preceding newline if they don't start with one,
    // to separate from previous command output.
    // However, GetPromptString should NOT include leading newline to avoid double spacing issue.
    // DisplayPrompt adds one newline before printing the prompt.
    // Update: User requested to remove automatic newline.
    printf("%s", buffer);
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
        printf("  push <local> [remote]    Push file to device (default: /storage/emulated/0/)\n");
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
        printf("  shizuku                  Activate Shizuku (requires installed app)\n");
        printf("  theme <name>             Switch prompt theme (robbyrussell/agnoster/minimal/pure)\n");
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
    printf("  cmd                      Enter Windows Command Prompt (type 'exit' to return)\n");
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
    } else if (strcmp(subcommand, "shizuku") == 0) {
        return CmdShizuku(state, &subcmd);
    } else if (strcmp(subcommand, "theme") == 0) {
        return CmdTheme(state, &subcmd);
    }

    return 0; // Not an ADB command
}

// Command: adb <command> [args...]
int CmdAdb(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        printf("Usage: adb <command> [args...]\n");
        printf("Commands: devices, select, info, push, pull, ls, rm, mkdir, shell, install, uninstall, reboot, dli, shizuku\n");
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
    {"8", "push", "Push file (requires local [remote])"},
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
    } else if (strcmp(cmd->name, "cmd") == 0) {
        return CmdCmd(state, cmd);
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

    int count = sscanf(cmd->args, "%s %s", local_path, remote_path);

    if (count < 1) {
        PrintError(ADB_ERROR_INVALID_COMMAND, "Usage: push <local> [remote]");
        return 1;
    }

    if (count == 1) {
        // Default to /storage/emulated/0/ if no remote path specified
        strcpy(remote_path, "/storage/emulated/0/");
        printf("No remote path specified, defaulting to: %s\n", remote_path);
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

// Command: shizuku
int CmdShizuku(AppState* state, const Command* cmd) {
    AdbDevice* device = GetSelectedDevice(state);
    if (!device) {
        PrintError(ADB_ERROR_NO_DEVICE, NULL);
        return 1;
    }

    printf("Activating Shizuku...\n");

    // 1. Get package path
    // adb shell pm path moe.shizuku.privileged.api
    ProcessResult* res = AdbShellCommand(state->adb_path, device->serial_id, "pm path moe.shizuku.privileged.api");
    if (!res || res->exit_code != 0 || !res->stdout_data || strlen(res->stdout_data) == 0) {
        PrintError(ADB_ERROR_UNKNOWN, "Shizuku app not found (package: moe.shizuku.privileged.api)");
        if (res) FreeProcessResult(res);
        return 1;
    }

    // Output: package:/data/app/~~.../base.apk
    char* path_start = strstr(res->stdout_data, "package:");
    if (!path_start) {
        PrintError(ADB_ERROR_UNKNOWN, "Unexpected output from pm path");
        FreeProcessResult(res);
        return 1;
    }
    path_start += 8; // Skip "package:"

    // Find end of line
    char* newline = strchr(path_start, '\n');
    if (newline) *newline = '\0';
    
    // Trim whitespace
    TrimString(path_start);

    // Remove /base.apk suffix
    char* base_apk = strstr(path_start, "/base.apk");
    if (base_apk) {
        *base_apk = '\0';
    } else {
        // Fallback: If not base.apk, try to strip the last file component (the apk name)
        char* last_slash = strrchr(path_start, '/');
        if (last_slash) {
            *last_slash = '\0';
        }
    }

    // Construct library path
    // Note: We try arm64 first as most modern devices are 64-bit.
    // Ideally we should check device ABI but this is a good heuristic for now.
    char lib_path[512];
    snprintf(lib_path, sizeof(lib_path), "%s/lib/arm64/libshizuku.so", path_start);

    printf("Found Shizuku path: %s\n", path_start);
    FreeProcessResult(res);

    // 2. Execute library
    printf("Executing: %s\n", lib_path);
    res = AdbShellCommand(state->adb_path, device->serial_id, lib_path);
    if (!res) {
        PrintError(ADB_ERROR_CONNECTION_FAILED, "Failed to execute activation command");
        return 1;
    }

    if (res->stdout_data && strlen(res->stdout_data) > 0) {
        printf("%s\n", res->stdout_data);
    }
    if (res->stderr_data && strlen(res->stderr_data) > 0) {
        fprintf(stderr, "%s\n", res->stderr_data);
    }

    if (res->exit_code == 0) {
        printf("Shizuku activation command executed successfully.\n");
    } else {
        printf("Shizuku activation command finished with exit code %d.\n", res->exit_code);
    }

    FreeProcessResult(res);
    return 1;
}

// Command: theme
int CmdTheme(AppState* state, const Command* cmd) {
    if (strlen(cmd->args) == 0) {
        // Interactive selection mode with live preview
        const char* theme_names[] = {
            "Default", "RobbyRussell", "Agnoster", "Minimal", "Pure",
            "Neon", "Dracula", "Matrix", "Cyberpunk", "Sunset",
            "Forest", "Ocean", "Retro", "Monokai", "Powerlevel",
            "Ubuntu", "Kali", "Arch", "Gentoo", "Root",
            "Debian", "Fedora", "CentOS", "Manjaro", "Mint",
            "Alpine", "StarWars", "Hacker", "Glitch", "Alien",
            "MacOS", "FreeBSD", "Solaris", "Windows11", "MSDOS",
            "Fish", "ZshSimple", "BashSimple", "Cloud", "IronMan"
        };
        ThemeMode theme_values[] = {
            THEME_DEFAULT, THEME_ROBBYRUSSELL, THEME_AGNOSTER, THEME_MINIMAL, THEME_PURE,
            THEME_NEON, THEME_DRACULA, THEME_MATRIX, THEME_CYBERPUNK, THEME_SUNSET,
            THEME_FOREST, THEME_OCEAN, THEME_RETRO, THEME_MONOKAI, THEME_POWERLEVEL,
            THEME_UBUNTU, THEME_KALI, THEME_ARCH, THEME_GENTOO, THEME_ROOT,
            THEME_DEBIAN, THEME_FEDORA, THEME_CENTOS, THEME_MANJARO, THEME_MINT,
            THEME_ALPINE, THEME_STARWARS, THEME_HACKER, THEME_GLITCH, THEME_ALIEN,
            THEME_MACOS, THEME_FREEBSD, THEME_SOLARIS, THEME_WINDOWS11, THEME_MSDOS,
            THEME_FISH, THEME_ZSH_SIMPLE, THEME_BASH_SIMPLE, THEME_CLOUD, THEME_IRONMAN
        };
        int count = 40;
        int current_idx = 0;

        // Find current index
        for (int i = 0; i < count; i++) {
            if (state->current_theme == theme_values[i]) {
                current_idx = i;
                break;
            }
        }

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

        // Hide cursor
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(hOut, &cursorInfo);
        BOOL originalVisible = cursorInfo.bVisible;
        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(hOut, &cursorInfo);

        printf("\n");
        printf("========================================\n");
        printf("          Theme Selection\n");
        printf("========================================\n");
        printf("Use Arrow Keys to navigate, Enter to confirm, Esc to cancel\n");
        printf("\n");

        int header_lines = 6; // Number of lines before the theme list
        int visible_count = 10; // Max visible themes at once
        int scroll_offset = 0; // Current scroll offset

        // Adjust initial scroll offset if current theme is out of view
        if (current_idx >= visible_count) {
            scroll_offset = current_idx - visible_count + 1;
        }

        int running = 1;

        while (running) {
            // Adjust scroll offset based on current selection
            if (current_idx < scroll_offset) {
                scroll_offset = current_idx;
            } else if (current_idx >= scroll_offset + visible_count) {
                scroll_offset = current_idx - visible_count + 1;
            }

            // Display theme list (windowed)
            for (int i = 0; i < visible_count; i++) {
                int idx = scroll_offset + i;
                if (idx >= count) break; // Should not happen if logic is correct

                if (idx == current_idx) {
                    printf(ANSI_GREEN "  > %s" ANSI_RESET " (current)\033[K\n", theme_names[idx]);
                } else {
                    printf("    %s\033[K\n", theme_names[idx]);
                }
            }

            // Display scroll indicators if needed
            // (Optional, but helpful: maybe add arrows in header/footer? 
            // For now let's keep it simple to match line counts)

            // Display preview section
            printf("\033[K\n");
            printf("────────────────────────────────────────\033[K\n");
            printf("Preview (command example):\033[K\n");

            // Temporarily set theme to preview
            ThemeMode saved_theme = state->current_theme;
            state->current_theme = theme_values[current_idx];
            char preview_prompt[256];
            GetPromptString(state, preview_prompt, sizeof(preview_prompt));
            state->current_theme = saved_theme; // Restore original theme

            int preview_newlines = CountNewlines(preview_prompt);
            
            printf("\033[0J"); // Clear everything below "Preview (command example):"
            
            printf("%s", preview_prompt);
            printf("install module.apk\n"); // Example command
            printf("────────────────────────────────────────\n");

            // Calculate preview lines:
            // 1. Empty line (Line 1093)
            // 2. Separator line (Line 1094)
            // 3. Title line (Line 1095)
            // 4. Prompt + Command line(s) (Line 1124-1125) -> preview_newlines + 1
            // 5. Bottom separator line (Line 1126)
            int lines_below_list = 5 + preview_newlines; 
            int total_lines_to_move_up = visible_count + lines_below_list;

            // Wait for input
            int ch = _getch();
            if (ch == 0 || ch == 224) { // Arrow keys
                int arrow = _getch();
                switch (arrow) {
                    case 72: // Up
                        current_idx--;
                        if (current_idx < 0) current_idx = count - 1;
                        break;
                    case 80: // Down
                        current_idx++;
                        if (current_idx >= count) current_idx = 0;
                        break;
                }
            } else if (ch == 13) { // Enter
                state->current_theme = theme_values[current_idx];
                SaveConfig(state->current_theme);

                // Clear the theme selection UI
                // We are currently at the bottom.
                printf("\033[%dA", total_lines_to_move_up + header_lines);
                printf("\033[0J");

                printf(ANSI_GREEN "✓" ANSI_RESET " Theme set to %s\n", theme_names[current_idx]);
                running = 0;
                break;
            } else if (ch == 27) { // Esc
                // Clear the theme selection UI
                printf("\033[%dA", total_lines_to_move_up + header_lines);
                printf("\033[0J");

                printf(ANSI_YELLOW "⊘" ANSI_RESET " Theme selection cancelled\n");
                running = 0;
                break;
            }

            // Move cursor back up to overwrite list and preview
            printf("\033[%dA", total_lines_to_move_up);
        }

        // Restore cursor
        cursorInfo.bVisible = originalVisible;
        SetConsoleCursorInfo(hOut, &cursorInfo);

        return 1;
    }

    char name[64];
    strncpy(name, cmd->args, sizeof(name)-1);
    StringToLower(name);

    ThemeMode new_theme = THEME_DEFAULT;

    if (strcmp(name, "default") == 0) new_theme = THEME_DEFAULT;
    else if (strcmp(name, "robbyrussell") == 0) new_theme = THEME_ROBBYRUSSELL;
    else if (strcmp(name, "agnoster") == 0) new_theme = THEME_AGNOSTER;
    else if (strcmp(name, "minimal") == 0) new_theme = THEME_MINIMAL;
    else if (strcmp(name, "pure") == 0) new_theme = THEME_PURE;
    else if (strcmp(name, "neon") == 0) new_theme = THEME_NEON;
    else if (strcmp(name, "dracula") == 0) new_theme = THEME_DRACULA;
    else if (strcmp(name, "matrix") == 0) new_theme = THEME_MATRIX;
    else if (strcmp(name, "cyberpunk") == 0) new_theme = THEME_CYBERPUNK;
    else if (strcmp(name, "sunset") == 0) new_theme = THEME_SUNSET;
    else if (strcmp(name, "forest") == 0) new_theme = THEME_FOREST;
    else if (strcmp(name, "ocean") == 0) new_theme = THEME_OCEAN;
    else if (strcmp(name, "retro") == 0) new_theme = THEME_RETRO;
    else if (strcmp(name, "monokai") == 0) new_theme = THEME_MONOKAI;
    else if (strcmp(name, "powerlevel") == 0) new_theme = THEME_POWERLEVEL;
    else if (strcmp(name, "ubuntu") == 0) new_theme = THEME_UBUNTU;
    else if (strcmp(name, "kali") == 0) new_theme = THEME_KALI;
    else if (strcmp(name, "arch") == 0) new_theme = THEME_ARCH;
    else if (strcmp(name, "gentoo") == 0) new_theme = THEME_GENTOO;
    else if (strcmp(name, "root") == 0) new_theme = THEME_ROOT;
    else if (strcmp(name, "debian") == 0) new_theme = THEME_DEBIAN;
    else if (strcmp(name, "fedora") == 0) new_theme = THEME_FEDORA;
    else if (strcmp(name, "centos") == 0) new_theme = THEME_CENTOS;
    else if (strcmp(name, "manjaro") == 0) new_theme = THEME_MANJARO;
    else if (strcmp(name, "mint") == 0) new_theme = THEME_MINT;
    else if (strcmp(name, "alpine") == 0) new_theme = THEME_ALPINE;
    else if (strcmp(name, "starwars") == 0) new_theme = THEME_STARWARS;
    else if (strcmp(name, "hacker") == 0) new_theme = THEME_HACKER;
    else if (strcmp(name, "glitch") == 0) new_theme = THEME_GLITCH;
    else if (strcmp(name, "alien") == 0) new_theme = THEME_ALIEN;
    else if (strcmp(name, "macos") == 0) new_theme = THEME_MACOS;
    else if (strcmp(name, "freebsd") == 0) new_theme = THEME_FREEBSD;
    else if (strcmp(name, "solaris") == 0) new_theme = THEME_SOLARIS;
    else if (strcmp(name, "windows11") == 0) new_theme = THEME_WINDOWS11;
    else if (strcmp(name, "msdos") == 0) new_theme = THEME_MSDOS;
    else if (strcmp(name, "fish") == 0) new_theme = THEME_FISH;
    else if (strcmp(name, "zshsimple") == 0) new_theme = THEME_ZSH_SIMPLE;
    else if (strcmp(name, "bashsimple") == 0) new_theme = THEME_BASH_SIMPLE;
    else if (strcmp(name, "cloud") == 0) new_theme = THEME_CLOUD;
    else if (strcmp(name, "ironman") == 0) new_theme = THEME_IRONMAN;
    else {
        printf("Unknown theme: %s\n", name);
        return 1;
    }

    state->current_theme = new_theme;
    SaveConfig(new_theme);
    printf("Theme set to %s\n", name);
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

// Command: cmd
int CmdCmd(AppState* state, const Command* cmd) {
    printf("Entering Windows Command Prompt...\n");
    printf("Type 'exit' to return to FolkADB.\n");
    
    // Save current title
    char old_title[MAX_PATH] = {0};
    GetConsoleTitleA(old_title, sizeof(old_title));
    
    SetConsoleTitleA("Windows Command Prompt (FolkADB)");
    
    system("cmd");
    
    // Restore title
    if (old_title[0]) {
        SetConsoleTitleA(old_title);
    }
    
    // Clear screen and restore banner
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

// Count newlines in a string
static int CountNewlines(const char* str) {
    int count = 0;
    while (*str) {
        if (*str == '\n') count++;
        str++;
    }
    return count;
}

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

// Clear multiline prompt and input
void ClearMultilinePrompt(const char* prompt) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) {
        ClearLine();
        return;
    }

    // Count newlines in prompt
    int newlines = CountNewlines(prompt);

    // Move cursor up to the start of the prompt
    if (newlines > 0) {
        printf("\033[%dA", newlines);
    }

    // Move to beginning of line
    printf("\r");

    // Clear from cursor to end of screen
    printf("\033[0J");
}

static const char* ADB_COMMANDS[] = {
    "devices", "dev", "select", "info", "push", "pull", "ls", "rm", "mkdir",
    "shell", "sudo", "install", "uninstall", "reboot", "dli", "shizuku", "theme",
    "help", "version", "cls", "cmd", "exit", "quit", NULL
};

static const char* FASTBOOT_COMMANDS[] = {
    "devices", "select", "info", "flash", "erase", "format", "unlock",
    "lock", "oem", "reboot", "getvar", "activate", "wipe",
    "help", "version", "cls", "cmd", "exit", "quit", NULL
};

static const char* REBOOT_MODES[] = {
    "system", "bootloader", "recovery", "fastboot", "edl", NULL
};

static const char* FLASH_PARTITIONS[] = {
    "boot", "recovery", "system", "userdata", "vbmeta", "vendor", 
    "init_boot", "vendor_boot", "dtbo", "super", "radio", "modem", NULL
};

static void HandleTabCompletion(AppState* state, char* input, int* input_pos) {
    // 1. Find the word being typed (last word segment)
    // We assume the cursor is at the end of input
    char* start_of_word = input + *input_pos;
    while (start_of_word > input && !isspace(*(start_of_word - 1))) {
        start_of_word--;
    }
    
    int word_len = (int)(input + *input_pos - start_of_word);
    char word_to_complete[256];
    if (word_len >= sizeof(word_to_complete)) return;
    strncpy(word_to_complete, start_of_word, word_len);
    word_to_complete[word_len] = '\0';

    // 2. Determine context (what is before start_of_word?)
    // Scan backwards from start_of_word to find previous word
    char* prev_word_end = start_of_word;
    while (prev_word_end > input && isspace(*(prev_word_end - 1))) {
        prev_word_end--;
    }
    
    char prev_word[64] = {0};
    
    if (prev_word_end > input) {
        // There is a previous word
        char* prev_word_start = prev_word_end;
        while (prev_word_start > input && !isspace(*(prev_word_start - 1))) {
            prev_word_start--;
        }
        
        int prev_len = (int)(prev_word_end - prev_word_start);
        if (prev_len < sizeof(prev_word)) {
            strncpy(prev_word, prev_word_start, prev_len);
            prev_word[prev_len] = '\0';
        }
    }

    // 3. Select candidates
    const char* matches[64];
    int match_count = 0;
    
    if (strlen(prev_word) == 0) {
        // Root command: Use current mode commands + extras
        const char** mode_cmds = (state->current_mode == MODE_FASTBOOT) ? FASTBOOT_COMMANDS : ADB_COMMANDS;
        static const char* ROOT_EXTRAS[] = { "adb", "fastboot", NULL };
        
        // Add mode commands
        for (int i = 0; mode_cmds[i] != NULL; i++) {
            if (strncmp(word_to_complete, mode_cmds[i], word_len) == 0) {
                if (match_count < 64) matches[match_count++] = mode_cmds[i];
            }
        }
        // Add extras (adb, fastboot)
        for (int i = 0; ROOT_EXTRAS[i] != NULL; i++) {
            if (strncmp(word_to_complete, ROOT_EXTRAS[i], word_len) == 0) {
                // Check duplicate
                int dup = 0;
                for(int j=0; j<match_count; j++) if(strcmp(matches[j], ROOT_EXTRAS[i])==0) dup=1;
                if (!dup && match_count < 64) matches[match_count++] = ROOT_EXTRAS[i];
            }
        }
    } else if (strcmp(prev_word, "adb") == 0) {
        // ADB Subcommands
        for (int i = 0; ADB_COMMANDS[i] != NULL; i++) {
            if (strncmp(word_to_complete, ADB_COMMANDS[i], word_len) == 0) {
                if (match_count < 64) matches[match_count++] = ADB_COMMANDS[i];
            }
        }
    } else if (strcmp(prev_word, "fastboot") == 0) {
        // Fastboot Subcommands
        for (int i = 0; FASTBOOT_COMMANDS[i] != NULL; i++) {
            if (strncmp(word_to_complete, FASTBOOT_COMMANDS[i], word_len) == 0) {
                if (match_count < 64) matches[match_count++] = FASTBOOT_COMMANDS[i];
            }
        }
    } else if (strcmp(prev_word, "reboot") == 0) {
        // Reboot modes
        for (int i = 0; REBOOT_MODES[i] != NULL; i++) {
            if (strncmp(word_to_complete, REBOOT_MODES[i], word_len) == 0) {
                if (match_count < 64) matches[match_count++] = REBOOT_MODES[i];
            }
        }
    } else if (strcmp(prev_word, "flash") == 0 || strcmp(prev_word, "erase") == 0 || strcmp(prev_word, "format") == 0 || strcmp(prev_word, "wipe") == 0) {
        // Partitions
        for (int i = 0; FLASH_PARTITIONS[i] != NULL; i++) {
            if (strncmp(word_to_complete, FLASH_PARTITIONS[i], word_len) == 0) {
                if (match_count < 64) matches[match_count++] = FLASH_PARTITIONS[i];
            }
        }
    }

    if (match_count == 0) {
        // No matches found, try fuzzy matching (typo correction)
        const char* best_match = NULL;
        int min_dist = 999;
        
        // Define candidates list again (union of all possible commands for this context)
        const char* candidates[128];
        int candidate_count = 0;

        if (strlen(prev_word) == 0) {
            const char** mode_cmds = (state->current_mode == MODE_FASTBOOT) ? FASTBOOT_COMMANDS : ADB_COMMANDS;
            static const char* ROOT_EXTRAS[] = { "adb", "fastboot", NULL };
            for (int i = 0; mode_cmds[i] != NULL; i++) candidates[candidate_count++] = mode_cmds[i];
            for (int i = 0; ROOT_EXTRAS[i] != NULL; i++) candidates[candidate_count++] = ROOT_EXTRAS[i];
        } else if (strcmp(prev_word, "adb") == 0) {
            for (int i = 0; ADB_COMMANDS[i] != NULL; i++) candidates[candidate_count++] = ADB_COMMANDS[i];
        } else if (strcmp(prev_word, "fastboot") == 0) {
            for (int i = 0; FASTBOOT_COMMANDS[i] != NULL; i++) candidates[candidate_count++] = FASTBOOT_COMMANDS[i];
        } else if (strcmp(prev_word, "reboot") == 0) {
            for (int i = 0; REBOOT_MODES[i] != NULL; i++) candidates[candidate_count++] = REBOOT_MODES[i];
        } else if (strcmp(prev_word, "flash") == 0 || strcmp(prev_word, "erase") == 0 || strcmp(prev_word, "format") == 0 || strcmp(prev_word, "wipe") == 0) {
             for (int i = 0; FLASH_PARTITIONS[i] != NULL; i++) candidates[candidate_count++] = FLASH_PARTITIONS[i];
        }

        for (int i = 0; i < candidate_count; i++) {
            int dist = LevenshteinDistance(word_to_complete, candidates[i]);
            if (dist != -1 && dist < min_dist) {
                min_dist = dist;
                best_match = candidates[i];
            }
        }

        // Heuristic: If distance is small enough relative to string length
        // e.g., distance <= 2 for strings > 3 chars
        if (best_match && strlen(word_to_complete) >= 2) {
            int threshold = 2;
            if (strlen(word_to_complete) > 5) threshold = 3;
            
            if (min_dist <= threshold) {
                 // Suggest correction
                 // Don't auto-replace immediately, just treat as single match logic
                 matches[0] = best_match;
                 match_count = 1;
            }
        }

        if (match_count == 0) return;
    }

    if (match_count == 1) {
        // Single match - complete it
        // We need to replace the part after start_of_word with the match
        strcpy(start_of_word, matches[0]);
        strcat(input, " "); 
        *input_pos = (int)strlen(input);
    } else {
        // Multiple matches - list them
        printf("\n");
        for (int i = 0; i < match_count; i++) {
            printf("%s  ", matches[i]);
        }
        printf("\n");
        
        // Find common prefix to complete as much as possible
        int prefix_len = word_len;
        while (1) {
            char c = matches[0][prefix_len];
            if (c == '\0') break; // End of first match
            
            int all_match = 1;
            for (int i = 1; i < match_count; i++) {
                if (matches[i][prefix_len] != c) {
                    all_match = 0;
                    break;
                }
            }
            
            if (all_match) {
                start_of_word[prefix_len - word_len] = c; // Not quite right indexing
                // Let's just append to input
                input[*input_pos] = c;
                *input_pos += 1;
                input[*input_pos] = '\0';
                
                prefix_len++;
            } else {
                break;
            }
        }
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
    
    // Track last displayed prompt to avoid unnecessary repaints
    char last_displayed_prompt[256] = {0};

    while (1) {
        // Show prompt if not shown or needs refresh
        if (!prompt_shown || g_prompt_needs_refresh) {
            
            // Check if we actually need to repaint (content changed)
            if (g_prompt_needs_refresh && prompt_shown) {
                char check_prompt[256];
                GetPromptString(state, check_prompt, sizeof(check_prompt));
                if (strcmp(check_prompt, last_displayed_prompt) == 0) {
                    // Content is identical, skip repaint to prevent flickering/stacking
                    g_prompt_needs_refresh = 0;
                    goto input_check;
                }
            }

            if (g_prompt_needs_refresh && prompt_shown) {
                // If refreshing (e.g. from thread), we need to clear properly.
                // New Approach: Use ANSI Erase in Display (J) which is more robust
                // for multi-line prompts than trying to clear line-by-line.

                // 1. Calculate height of the *previous* prompt (what's currently on screen)
                int prev_newlines = 0;
                for (int i = 0; last_displayed_prompt[i]; i++) {
                    if (last_displayed_prompt[i] == '\n') prev_newlines++;
                }

                // 2. Move cursor up to the start of the prompt
                if (prev_newlines > 0) {
                     printf("\033[%dA", prev_newlines);
                }
                
                // 3. Move to beginning of line
                printf("\r");
                
                // 4. Clear everything from cursor down
                printf("\033[0J");

                // 5. Get and print the new prompt
                char prompt[256];
                GetPromptString(state, prompt, sizeof(prompt));
                printf("%s", prompt); 
                
                // Update tracker
                strncpy(last_displayed_prompt, prompt, sizeof(last_displayed_prompt)-1);
            } else {
                DisplayPrompt(state);
                // Update tracker
                char p[256];
                GetPromptString(state, p, sizeof(p));
                strncpy(last_displayed_prompt, p, sizeof(last_displayed_prompt)-1);
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

input_check:;
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
                                    char prompt_str[256];
                                    GetPromptString(state, prompt_str, sizeof(prompt_str));

                                    // Check if prompt is multiline
                                    int prompt_newlines = CountNewlines(prompt_str);
                                    if (prompt_newlines > 0) {
                                        // Use multiline-aware clear
                                        ClearMultilinePrompt(prompt_str);
                                    } else {
                                        // Use simple clear for single-line prompts
                                        ClearLine();
                                    }

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
                        // Handle Tab
                        else if (vk == VK_TAB) {
                             HandleTabCompletion(state, input, &input_pos);
                             needs_repaint = 1;
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
                    char prompt_str[256];
                    GetPromptString(state, prompt_str, sizeof(prompt_str));

                    // Check if prompt is multiline
                    int newlines = CountNewlines(prompt_str);
                    if (newlines > 0) {
                        // Use multiline-aware clear
                        ClearMultilinePrompt(prompt_str);
                    } else {
                        // Use simple clear for single-line prompts
                        ClearLine();
                    }

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
