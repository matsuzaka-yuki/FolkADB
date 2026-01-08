#ifndef MODULE_INSTALLER_H
#define MODULE_INSTALLER_H

#include "common.h"

// Root solution types
typedef enum {
    ROOT_NONE,
    ROOT_MAGISK,
    ROOT_KSU,
    ROOT_APATCH
} RootSolution;

// Check if zip file is a module (contains module.prop)
int IsModuleZip(const char* zip_path, const char* seven_zip_path);

// Detect root solution
RootSolution DetectRootSolution(AppState* state);

// Install module
void InstallRootModule(AppState* state, const char* remote_zip_path, RootSolution solution);

// Download file from URL to destination
int DownloadFile(const char* url, const char* dest_path);

#endif // MODULE_INSTALLER_H
