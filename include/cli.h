#ifndef CLI_H
#define CLI_H

#include "common.h"

// Command structure
typedef struct {
    char name[64];
    char args[256];
} Command;

// CLI functions
void RunInteractiveLoop(AppState* state);
Command ParseCommand(const char* input);
int ExecuteCommand(AppState* state, const Command* cmd);
void ShowHelp(AppState* state);
void DisplayPrompt(const AppState* state);
void ShowBanner(void);

// Command handlers
int CmdAdb(AppState* state, const Command* cmd);
int CmdDevices(AppState* state, const Command* cmd);
int CmdSelect(AppState* state, const Command* cmd);
int CmdInfo(AppState* state, const Command* cmd);
int CmdShell(AppState* state, const Command* cmd);
int CmdPush(AppState* state, const Command* cmd);
int CmdPull(AppState* state, const Command* cmd);
int CmdInstall(AppState* state, const Command* cmd);
int CmdUninstall(AppState* state, const Command* cmd);
int CmdLs(AppState* state, const Command* cmd);
int CmdRm(AppState* state, const Command* cmd);
int CmdMkdir(AppState* state, const Command* cmd);
int CmdSudo(AppState* state, const Command* cmd);
int CmdReboot(AppState* state, const Command* cmd);
int CmdHelp(AppState* state, const Command* cmd);
int CmdVersion(AppState* state, const Command* cmd);
int CmdCls(AppState* state, const Command* cmd);

// Fastboot command handlers
int CmdFbDevices(AppState* state, const Command* cmd);
int CmdFbSelect(AppState* state, const Command* cmd);
int CmdFbInfo(AppState* state, const Command* cmd);
int CmdFbFlash(AppState* state, const Command* cmd);
int CmdFbErase(AppState* state, const Command* cmd);
int CmdFbFormat(AppState* state, const Command* cmd);
int CmdFbUnlock(AppState* state, const Command* cmd);
int CmdFbLock(AppState* state, const Command* cmd);
int CmdFbOem(AppState* state, const Command* cmd);
int CmdFbReboot(AppState* state, const Command* cmd);
int CmdFbGetvar(AppState* state, const Command* cmd);
int CmdFbActivate(AppState* state, const Command* cmd);
int CmdFbWipe(AppState* state, const Command* cmd);

#endif // CLI_H
