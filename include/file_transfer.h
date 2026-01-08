#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include "common.h"

// File transfer functions
int PushFile(AppState* state, const char* local_path, const char* remote_path);
int PullFile(AppState* state, const char* remote_path, const char* local_path);
int ListRemoteFiles(AppState* state, const char* remote_path);
int DeleteRemoteFile(AppState* state, const char* remote_path);
int CreateRemoteDirectory(AppState* state, const char* remote_path);

#endif // FILE_TRANSFER_H
